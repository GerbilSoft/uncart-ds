// Copyright 2014 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

/**
 * References:
 * - http://problemkaputt.de/gbatek.htm#dscartridgesencryptionfirmware
 * - http://problemkaputt.de/gbatek.htm#dscartridgeprotocol
 * - https://www.3dbrew.org/wiki/Gamecards
 */

#include <stdbool.h>
#include "command_ntr.h"
#include "protocol_ntr.h"

void NTR_CmdReset(void)
{
    static const u32 reset_cmd[2] = { 0x9F000000, 0x00000000 };
    NTR_SendCommand(reset_cmd, 0x2000, NTRCARD_CLK_SLOW | NTRCARD_DELAY1(0x1FFF) | NTRCARD_DELAY2(0x18), NULL);
}

u32 NTR_CmdGetCartId(void)
{
    u32 id;
    static const u32 getid_cmd[2] = { 0x90000000, 0x00000000 };
    NTR_SendCommand(getid_cmd, 0x4, NTRCARD_CLK_SLOW | NTRCARD_DELAY1(0x1FFF) | NTRCARD_DELAY2(0x18), &id);
    return id;
}

void NTR_CmdEnter16ByteMode(void)
{
    static const u32 enter16bytemode_cmd[2] = { 0x3E000000, 0x00000000 };
    NTR_SendCommand(enter16bytemode_cmd, 0x0, 0, NULL);
}

// FIXME: What should the correct latency parameter be?
// Using "slow" values from NTR_CmdGetCartId() for now.

/**
 * Read the header of an NTR/TWL cartridge.
 * @param buffer Header buffer. (Must be >= 0x200 bytes)
 */
void NTR_CmdReadHeader(void* buffer)
{
    static const u32 readheader_cmd[2] = { 0x00000000, 0x00000000 };
    NTR_SendCommand(readheader_cmd, 0x200, NTRCARD_CLK_SLOW | NTRCARD_DELAY1(0x1FFF) | NTRCARD_DELAY2(0x18), buffer);
}

/** KEY1/KEY2 encryption code based on Wood Dumper. **/

#define ALIGN(n) __attribute__ ((aligned (n)))
#include "keys.h"
#include "delay.h"
#include "protocol.h"   // Cart_GetID()

enum {
    EInitAreaSize = 0x8000,
    ESecureAreaSize = 0x4000,
    ESecureAreaOffset = 0x4000,
    EBufferSize = 0x8000,
    EBlockSize = 0x200,
    EHashSize = 0x412,
    EEncMudule = 2
};

static u32 iCardHash[EHashSize];
static u32 iKeyCode[3];

// lol
#define getRandomNumber() (4)

// KEY1 context.
struct {
    unsigned int iii;
    unsigned int jjj;
    unsigned int kkkkk;
    unsigned int llll;
    unsigned int mmm;
    unsigned int nnn;
} iKey1;

static void cryptUp(u32* aPtr)
{
    u32 x = aPtr[1];
    u32 y = aPtr[0];
    u32 z;
    for (int ii = 0; ii < 0x10; ++ii) {
        z = iCardHash[ii]^x;
        x = iCardHash[0x012+((z>>24)&0xff)];
        x = iCardHash[0x112+((z>>16)&0xff)]+x;
        x = iCardHash[0x212+((z>>8)&0xff)]^x;
        x = iCardHash[0x312+((z>>0)&0xff)]+x;
        x = y^x;
        y = z;
    }
    aPtr[0] = x^iCardHash[0x10];
    aPtr[1] = y^iCardHash[0x11];
}

static void cryptDown(u32* aPtr)
{
    u32 x = aPtr[1];
    u32 y = aPtr[0];
    u32 z;
    for (int ii = 0x11; ii > 0x01; --ii) {
        z = iCardHash[ii]^x;
        x = iCardHash[0x012+((z>>24)&0xff)];
        x = iCardHash[0x112+((z>>16)&0xff)]+x;
        x = iCardHash[0x212+((z>>8)&0xff)]^x;
        x = iCardHash[0x312+((z>>0)&0xff)]+x;
        x = y^x;
        y = z;
    }
    aPtr[0] = x^iCardHash[0x01];
    aPtr[1] = y^iCardHash[0x00];
}

static inline u32 bswap(u32 x)
{
    return (x >> 24) |
           (x >> 8 & 0x0000FF00) |
           (x << 8 & 0x00FF0000) |
           (x << 24);
}

/**
 * Apply the Key1 hash.
 */
static void applyKey(void)
{
    u32 scratch[2] = {0, 0};
    cryptUp(&iKeyCode[1]);
    cryptUp(&iKeyCode[0]);

    for (int ii = 0; ii < 0x12; ++ii) {
        iCardHash[ii] = iCardHash[ii] ^ bswap(iKeyCode[ii%2]);
    }
    for (int ii=0;ii<EHashSize;ii+=2) {
        cryptUp(scratch);
        iCardHash[ii] = scratch[1];
        iCardHash[ii+1] = scratch[0];
    }
}

#include "draw.h"
/**
 * Initialize the Key1 hash.
 * @param gameCode 32-bit game code.
 * @param aType If non-zero, call ApplyKey() a third time.
 */
static void initKey(u32 gameCode, int aType)
{
    // TODO: Use TWL_BF_Key1[] for DSi Secure Area.
    memset(iCardHash, 0, sizeof(iCardHash));
    memcpy(iCardHash, NTR_BF_Key1, sizeof(NTR_BF_Key1));
    iKeyCode[0] = gameCode;
    iKeyCode[1] = gameCode/2;
    iKeyCode[2] = gameCode*2;

    applyKey();
    applyKey();
    iKeyCode[1] *= 2;
    iKeyCode[2] /= 2;
    if (aType) {
	    applyKey();
    }
}

/**
 * Initialize the KEY1 context.
 * @param cmdData [out] Command data.
 */
static void initKey1(u8 cmdData[8])
{
    iKey1.iii=getRandomNumber()&0x00000fff;
    iKey1.jjj=getRandomNumber()&0x00000fff;
    iKey1.kkkkk=getRandomNumber()&0x000fffff;
    iKey1.llll=getRandomNumber()&0x0000ffff;
    iKey1.mmm=getRandomNumber()&0x00000fff;
    iKey1.nnn=getRandomNumber()&0x00000fff;

    cmdData[7] = NTRCARD_CMD_ACTIVATE_BF;
    cmdData[6] = (u8)(iKey1.iii>>4);
    cmdData[5] = (u8)((iKey1.iii<<4)|(iKey1.jjj>>8));
    cmdData[4] = (u8)iKey1.jjj;
    cmdData[3] = (u8)(iKey1.kkkkk>>16);
    cmdData[2] = (u8)(iKey1.kkkkk>>8);
    cmdData[1] = (u8)iKey1.kkkkk;
    cmdData[0] = (u8)getRandomNumber();
}

/**
 * Create a KEY1 encrypted command.
 * @param cmd		[in]  Command.
 * @param cmdData	[out] Command data.
 * @param block		[in]  Block number.
 */
void createEncryptedCommand(u8 cmd, u8 cmdData[8], u32 block)
{
    u32 iii, jjj;
    if (cmd != NTRCARD_CMD_SECURE_READ) {
        // Block number is only used for secure area read.
        // For everything else, it's a KEY1 context value.
        block = iKey1.llll;
    }

    if (cmd == NTRCARD_CMD_ACTIVATE_SEC) {
      iii = iKey1.mmm;
      jjj = iKey1.nnn;
    } else {
      iii = iKey1.iii;
      jjj = iKey1.jjj;
    }

    // Fill in the command data.
    cmdData[7] = (u8)(cmd|(block>>12));
    cmdData[6] = (u8)(block>>4);
    cmdData[5] = (u8)((block<<4)|(iii>>8));
    cmdData[4] = (u8)iii;
    cmdData[3] = (u8)(jjj>>4);
    cmdData[2] = (u8)((jjj<<4)|(iKey1.kkkkk>>16));
    cmdData[1] = (u8)(iKey1.kkkkk>>8);
    cmdData[0] = (u8)iKey1.kkkkk;
    Debug("NoEnc: %02X%02X%02X%02X %02X%02X%02X%02X",
          cmdData[7], cmdData[6], cmdData[5], cmdData[4],
          cmdData[3], cmdData[2], cmdData[1], cmdData[0]);
    cryptUp((u32*)cmdData);
    iKey1.kkkkk += 1;

    Debug("Enc:   %02X%02X%02X%02X %02X%02X%02X%02X",
          cmdData[7], cmdData[6], cmdData[5], cmdData[4],
          cmdData[3], cmdData[2], cmdData[1], cmdData[0]);
}

/**
 * Decrypt Secure Area data. (first 2 KB)
 * @param gameCode	[in] Game code.
 * @param secureArea	[in/out] Secure area.
 */
void decryptSecureArea(u32 gameCode, u32 *secureArea)
{
    initKey(gameCode, 0);
    cryptDown(secureArea);
    initKey(gameCode, 1);
    for (int ii = 0; ii < 0x200; ii += 2) {
        cryptDown(secureArea + ii);
    }
}

/**
 * Get the secure chip ID.
 * Only usable when reading the Secure Area.
 * @param flagsKey1 [in] KEY1 flags.
 * @param chip_id   [in] Original chip ID.
 * @param ntrHeader [in]  NTR header. (TODO: Eliminate this?)
 * @return True if the secure chip ID matches; false if not.
 */
static bool NTR_GetSecureChipID(u32 flagsKey1, u32 chip_id, const NTR_HEADER *ntrHeader)
{
    ALIGN(4) u8 cmdData[8];
    createEncryptedCommand(NTRCARD_CMD_SECURE_CHIPID, cmdData, 0);

    if (chip_id & 0x80000000) {
        // Extra delay and command is required.
        // secure_area_delay is in 131 kHz units. Convert to microseconds.
        // TODO: Optimize this by not using floating-point arithmetic.
        NTR_SendCommand8(cmdData, 0, flagsKey1, NULL);
        u32 us = ntrHeader->secure_area_delay * 7.63;
        ioDelay(us);
    }

    // Request the secure chip ID.
    u32 secure_chip_id = 0;
    NTR_SendCommand8(cmdData, 4, flagsKey1, &secure_chip_id);
    Debug("Secure chip ID: %08X", secure_chip_id);
}

/**
 * Read the Secure Area. (0x4000-0x7FFF)
 * This switches into KEY2 encryption mode.
 * @param buffer	[out] Output buffer.
 * @param ntrHeader	[in]  NTR header.
 */
void NTR_ReadSecureArea(void *buffer, const NTR_HEADER *ntrHeader)
{
    // Partially based on Wood Dumper.
    u32 gameCode;
    memcpy(&gameCode, ntrHeader->gamecode, sizeof(gameCode));
    initKey(gameCode, false);

    // Determine which sub-protocol to use.
    // "Large" cartridges return the secure area in 0x200-byte chunks.
    // "Small" cartridges return the secure area in 0x1000-byte chunks.
    u32 chip_id = Cart_GetID();
    bool isLargeCart = !!(chip_id & 0x80000000);
        
    u32 flagsKey1 = NTRCARD_ACTIVATE | NTRCARD_nRESET |
                    (ntrHeader->cardControl13 & (NTRCARD_WR | NTRCARD_CLK_SLOW)) |
                    ((ntrHeader->cardControlBF & (NTRCARD_CLK_SLOW | NTRCARD_DELAY1(0x1FFF))) +
                    ((ntrHeader->cardControlBF & NTRCARD_DELAY2(0x3F))>>16));
    u32 flagsSec = (ntrHeader->cardControlBF & (NTRCARD_CLK_SLOW | NTRCARD_DELAY1(0x1FFF) |NTRCARD_DELAY2(0x3F))) |
                    NTRCARD_ACTIVATE | NTRCARD_nRESET | NTRCARD_SEC_EN | NTRCARD_SEC_DAT;
    u32 flags = ntrHeader->cardControl13 & ~NTRCARD_BLK_SIZE(7);
    if (!isLargeCart) {
        // "Small" cart. Read the secure area in 0x1000-byte chunks.
        flagsKey1 |= NTRCARD_SEC_LARGE;
    }

    ALIGN(4) u8 cmdData[8];

    // Activate KEY1 mode.
    memset(cmdData, 0, sizeof(cmdData));
    initKey1(cmdData);
    NTR_SendCommand8(cmdData, 0, (ntrHeader->cardControl13 & (NTRCARD_WR | NTRCARD_nRESET | NTRCARD_CLK_SLOW)) | NTRCARD_ACTIVATE, NULL);

    // Activate KEY2 mode.
    createEncryptedCommand(NTRCARD_CMD_ACTIVATE_SEC, cmdData, 0);
    NTR_SendCommand8(cmdData, 0, flagsKey1, NULL);
    if (isLargeCart) {
        // Extra delay and command is required.
        // secure_area_delay is in 131 kHz units. Convert to microseconds.
        // TODO: Optimize this by not using floating-point arithmetic.
        u32 us = ntrHeader->secure_area_delay * 7.63;
        ioDelay(us);
        NTR_SendCommand8(cmdData, 0, flagsKey1, NULL);
    }

    // Set the KEY1 seed for the Secure Area.
    static const u8 cardSeedBytes[] = {0xE8, 0x4D, 0x5A, 0xB1, 0x17, 0x8F, 0x99, 0xD5};
    REG_NTRCARDROMCNT = 0;
    REG_NTRCARDSEEDX_L = cardSeedBytes[ntrHeader->enc_seed_select & 0x07] | (iKey1.nnn<<15) | (iKey1.mmm<<27) | 0x6000;
    REG_NTRCARDSEEDY_L = 0x879B9B05;
    REG_NTRCARDSEEDX_H = iKey1.mmm>>5;
    REG_NTRCARDSEEDY_H = 0x5C;
    REG_NTRCARDROMCNT = NTRCARD_nRESET | NTRCARD_SEC_SEED | NTRCARD_SEC_EN | NTRCARD_SEC_DAT;
    flagsKey1 |= NTRCARD_SEC_EN | NTRCARD_SEC_DAT;

    // Get the Secure chip ID.
    // FIXME: Not working correctly???
    bool isOk = NTR_GetSecureChipID(flagsKey1, chip_id, ntrHeader);

    // Switch to regular data mode.
    createEncryptedCommand(NTRCARD_CMD_DATA_MODE, cmdData, 0);
    // TODO: isLargeCart
    NTR_SendCommand8(cmdData, 0, flagsKey1, NULL);

    // Read four bytes at 0x8000.
    // NOTE: Still not working...
    u32 flagsRead = flags | NTRCARD_ACTIVATE | NTRCARD_nRESET;
    u32 data = 0;
    cmdData[7] = NTRCARD_CMD_DATA_READ;
    cmdData[6] = 0x00;
    cmdData[5] = 0x00;
    cmdData[4] = 0x80;
    cmdData[3] = 0x00;
    cmdData[2] = 0x00;
    cmdData[1] = 0x00;
    cmdData[0] = 0x00;
    NTR_SendCommand8(cmdData, 4, flagsRead, &data);
    Debug("read 0x8000: %08X", data);
}
