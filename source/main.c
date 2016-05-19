/**
 * References:
 * - http://problemkaputt.de/gbatek.htm#dscartridgesencryptionfirmware
 * - http://problemkaputt.de/gbatek.htm#dscartridgeprotocol
 * - https://www.3dbrew.org/wiki/Gamecards
 */

#include "draw.h"
#include "hid.h"
#include "fatfs/ff.h"
#include "gamecart/protocol.h"
#include "gamecart/protocol_ctr.h"
#include "gamecart/protocol_ntr.h"
#include "gamecart/command_ctr.h"
#include "gamecart/command_ntr.h"
#include "headers.h"
#include "i2c.h"

#include <string.h>
#include <stdio.h>

static void poweroff()
{
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 0);
}

// File IO utility functions
static FATFS fs;
static FIL file;

static void ClearTop(void) {
    ClearScreen(TOP_SCREEN1, RGB(255, 255, 255));
    current_y = 0;
}

static void wait_key(void) {
    Debug("Press key to continue...");
    InputWait();
}

struct Context {
    u8* buffer;
    size_t buffer_size;

    u32 cart_size;
    u32 media_unit;
};

static int dump_ctr_cart_region(u32 start_sector, u32 end_sector, FIL* output_file, struct Context* ctx)
{
    u32 read_size = 1u * 1024 * 1024 / ctx->media_unit; // 1MiB default

    // Dump remaining data
    u32 current_sector = start_sector;
    while (current_sector < end_sector) {
        unsigned int percentage = current_sector * 100 / ctx->cart_size;
        Debug("Dumping %08X / %08X - %3u%%", current_sector, ctx->cart_size, percentage);

        u8* read_ptr = ctx->buffer;
        while (read_ptr < ctx->buffer + ctx->buffer_size && current_sector < end_sector) {
            CTR_Dummy();
            CTR_Dummy();
            
	    //If there is less data to read than the curren read_size, fix it
	    if (end_sector - current_sector < read_size)
            {
                read_size = end_sector - current_sector;
            }
            CTR_CmdReadData(current_sector, ctx->media_unit, read_size, read_ptr);
            read_ptr += ctx->media_unit * read_size;
            current_sector += read_size;
        }

        u8* write_ptr = ctx->buffer;
        while (write_ptr < read_ptr) {
            unsigned int bytes_written = 0;
            f_write(output_file, write_ptr, (size_t)(read_ptr - write_ptr), &bytes_written);
            Debug("Wrote 0x%x bytes...", bytes_written);

            if (bytes_written == 0) {
                Debug("Writing failed! :( SD full?");
                return -1;
            }

            write_ptr += bytes_written;
        }
    }

    return 0;
}

/**
 * Dump a CTR cartridge.
 */
static void dump_ctr()
{
    // Arbitrary target buffer
    // TODO: This should be done in a nicer way ;)
    u32* target = (u32*)0x22000000;
    NCSD_HEADER *ncsdHeader = (NCSD_HEADER*)target;
    u32 target_buf_size = 16u * 1024u * 1024u; // 16MB
    memset(target, 0, target_buf_size); // Clear our buffer

    u32* ncchHeaderData = (u32*)0x23000000;
    NCCH_HEADER *ncchHeader = (NCCH_HEADER*)ncchHeaderData;

    *(vu32*)0x10000020 = 0; // InitFS stuff
    *(vu32*)0x10000020 = 0x340; // InitFS stuff

    // ROM DUMPING CODE STARTS HERE

    Debug("Reading NCCH header...");
    CTR_CmdReadHeader(ncchHeader);
    Debug("Done reading NCCH header.");

    if (strncmp((const char*)(ncchHeader->magic), "NCCH", 4))
    {
        Debug("NCCH magic not found in header!!!");
        Debug("Press A to continue anyway.");
        if (!(InputWait() & BUTTON_A))
            return;
    }

    u32 sec_keys[4];
    CTR_Secure_Init(ncchHeaderData, sec_keys);

    // Guess 0x200 first for the media size. this will be set correctly once the cart header is read 
    // Read out the header 0x0000-0x1000
    CTR_Dummy();
    Debug("Reading NCSD header...");
    CTR_CmdReadData(0, 0x200, 0x1000 / 0x200, target);
    Debug("Done reading NCSD header.");

    if (strncmp((const char*)(ncsdHeader->magic), "NCSD", 4)) {
        Debug("NCSD magic not found in header!!!");
        Debug("Press A to continue anyway.");
        if (!(InputWait() & BUTTON_A))
            return;
    }

    const u32 mediaUnit = 0x200 * (1u << ncsdHeader->partition_flags[MEDIA_UNIT_SIZE]); //Correctly set the media unit size

    // Calculate the actual size by counting the adding the size of each partition, plus the initial offset
    // size is in media units
    // FIXME: Option to dump entire card instead of just the used area?
    u32 cartSize = ncsdHeader->offsetsize_table[0].offset;
    for (int i = 0; i < 8; i++) {
        cartSize += ncsdHeader->offsetsize_table[i].size;
    }

    Debug("Cart data size: %llu MB", (u64)cartSize * (u64)mediaUnit / 1024ull / 1024ull);

    struct Context context = {
        .buffer = (u8*)target,
        .buffer_size = target_buf_size,
        .cart_size = cartSize,
        .media_unit = mediaUnit,
    };

    // Maximum number of blocks in a single file
    u32 file_max_blocks = 0xFFFFFFFFu / mediaUnit; // 4GiB - 1
    u32 current_part = 0;

    while (current_part * file_max_blocks < cartSize) {
        // Create output file
        char filename_buf[32];
        char extension_digit = cartSize <= file_max_blocks ? 's' : '0' + current_part;
        // TODO: Add the ROM revision.
        snprintf(filename_buf, sizeof(filename_buf), "/%.16s.3d%c",
                 ncchHeader->product_code, extension_digit);
        Debug("Writing to file: \"%s\"", filename_buf);
        Debug("Change the SD card now and/or press a key.");
        Debug("(Or SELECT to cancel)");
        if (InputWait() & BUTTON_SELECT)
            break;

        if (f_mount(&fs, "0:", 0) != FR_OK) {
            Debug("Failed to f_mount... Retrying");
            wait_key();
            goto cleanup_none;
        }

        if (f_open(&file, filename_buf, FA_READ | FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
            Debug("Failed to create file... Retrying");
            wait_key();
            goto cleanup_mount;
        }

        f_lseek(&file, 0);

        u32 region_start = current_part * file_max_blocks;
        u32 region_end = region_start + file_max_blocks;
        if (region_end > cartSize)
            region_end = cartSize;

        if (dump_ctr_cart_region(region_start, region_end, &file, &context) < 0)
            goto cleanup_file;

        if (current_part == 0) {
            // Write header - TODO: Not sure why this is done at the very end..
            f_lseek(&file, 0x1000);
            unsigned int written = 0;
            // Fill the 0x1200-0x4000 unused area with 0xFF instead of random garbage.
            memset((u8*)ncchHeaderData + 0x200, 0xFF, 0x3000 - 0x200);
            f_write(&file, ncchHeader, 0x3000, &written);
        }

        Debug("Done!");
        current_part += 1;

cleanup_file:
        // Done, clean up...
        f_sync(&file);
        f_close(&file);
cleanup_mount:
        f_mount(NULL, "0:", 0);
cleanup_none:
        ;
    }
}

/**
 * Dump an NTR cartridge.
 * @param chip_id Chip ID.
 */
static void dump_ntr(u32 chip_id)
{
    // Arbitrary target buffer
    // TODO: This should be done in a nicer way ;)
    u8* target = (u8*)0x22000000;
    NTR_HEADER *ntrHeader = (NTR_HEADER*)target;
    u32 target_buf_size = 16u * 1024u * 1024u; // 16MB
    memset(target, 0, target_buf_size); // Clear our buffer

    *(vu32*)0x10000020 = 0; // InitFS stuff
    *(vu32*)0x10000020 = 0x340; // InitFS stuff

    // First 8 KB of the NTR ROM:
    // - 0x0000-0x0FFF: Header. (0x0200 for NTR, 0x1000 for TWL)
    // - 0x1000-0x3FFF: Unreadable. (Fill with 0x00)
    // - 0x4000-0x7FFF: Secure area.

    Debug("Reading NTR header...");
    NTR_CmdReadHeader(ntrHeader);
    Debug("Done reading NTR header.");
    Debug("Game title: %.12s (Rev.%02u)", ntrHeader->game_title, ntrHeader->rom_version);

    // Get the ROM size from the chip ID.
    const u8 chip_id_sz = (chip_id >> 8) & 0xFF;
    u32 rom_size_mb;
    if (chip_id_sz < 0xF0) {
           // FIXME: Only 0x00-0x7F?
           rom_size_mb = chip_id_sz + 1;
    }

    // TODO: Print a warning if chip ID size != NTR_HEADER size.
    Debug("Device size: %u MB", rom_size_mb);
    if (ntrHeader->device_capacity <= 2) {
        // 512 KB or less.
        Debug("ROM size:    %u KB", 128 << ntrHeader->device_capacity);
        Debug("ROM usage:   %u KB", ntrHeader->total_used_rom_size >> 10);
    } else {
        // 1 MB or greater.
        Debug("ROM size:    %u MB", 1 << (ntrHeader->device_capacity - 3));
        Debug("ROM usage:   %u MB", ntrHeader->total_used_rom_size >> 20);
    }

    // Clear 0x1000-0x3FFF.
    memset(&target[0x1000], 0, 0x3000); // Clear our buffer

    // Read the secure area. (0x4000-0x7FFF)
    NTR_ReadSecureArea(&target[0x4000], ntrHeader);
}

int main()
{
    while (true) {
        // Setup boring stuff - clear the screen, initialize SD output, etc...
        ClearTop();

        Debug("Uncart-DS: ROM dump tool v0.2");
        Debug("Insert your game cart now.");
        wait_key();

        Cart_Init();
	u32 chip_id = Cart_GetID();
        Debug("Chip ID is %08X", chip_id);
	if (chip_id == 0x00000000 || chip_id == 0xFFFFFFFF) {
		Debug("Cartridge not found!");
		Debug("Press B to exit, any other key to restart.");
		if (!(InputWait() & BUTTON_B))
			continue;
		break;
	} else if (chip_id & 0x80000000) {
		// 3DS cartridge.
		dump_ctr();
	} else {
		// DS(i) cartridge.
		// FIXME: Proper TWL support.
		dump_ntr(chip_id);
	}

        Debug("Press B to exit, any other key to restart.");
        if (!(InputWait() & BUTTON_B))
            continue;
	break;
    }

    poweroff();
    return 0;
}
