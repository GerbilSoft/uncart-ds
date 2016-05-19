#ifndef UNCART_HEADERS_H_
#define UNCART_HEADERS_H_

#include "common.h"

typedef enum
{
	MEDIA_6X_SAVE_CRYPTO = 1,
	MEDIA_CARD_DEVICE = 3,
	MEDIA_PLATFORM_INDEX = 4,
	MEDIA_TYPE_INDEX = 5,
	MEDIA_UNIT_SIZE = 6,
	MEDIA_CARD_DEVICE_OLD = 7
} NcsdFlagIndex;

typedef struct
{
	u32 offset;
	u32 size;
} partition_offsetsize;

typedef struct
{
	u8 sha256[0x100];
	u8 magic[4];
	u32 media_size;
	u8 title_id[8];
	u8 partitions_fs_type[8];
	u8 partitions_crypto_type[8];
	partition_offsetsize offsetsize_table[8];
	u8 exheader_hash[0x20];
	u8 additional_header_size[0x4];
	u8 sector_zero_offset[0x4];
	u8 partition_flags[8];
	u8 partition_id_table[8][8];
	u8 reserved[0x30];
} NCSD_HEADER;

typedef struct
{
	u8 sha256[0x100];
	u8 magic[4];
	u32 content_size;
	u8 title_id[8];
	u8 maker_code[2];
	u8 version[2];
	u8 reserved_0[4];
	u8 program_id[8];
	u8 temp_flag;
	u8 reserved_1[0xF];
	u8 logo_sha_256_hash[0x20];
	u8 product_code[0x10];
	u8 extended_header_sha_256_hash[0x20];
	u8 extended_header_size[4];
	u8 reserved_2[4];
	u8 flags[8];
	u8 plain_region_offset[4];
	u8 plain_region_size[4];
	u8 logo_region_offset[4];
	u8 logo_region_size[4];
	u8 exefs_offset[4];
	u8 exefs_size[4];
	u8 exefs_hash_size[4];
	u8 reserved_4[4];
	u8 romfs_offset[4];
	u8 romfs_size[4];
	u8 romfs_hash_size[4];
	u8 reserved_5[4];
	u8 exefs_sha_256_hash[0x20];
	u8 romfs_sha_256_hash[0x20];
} __attribute__((__packed__))
NCCH_HEADER;

// Reference: http://problemkaputt.de/gbatek.htm#dscartridgeheader
typedef struct
{
	char game_title[12];
	char gamecode[4];
	char makercode[4];
	u8 unitcode;
	u8 enc_seed_select;
	u8 device_capacity;
	u8 reserved1[7];
	u8 reserved2_dsi;
	u8 nds_region;	// 0x00 == normal, 0x80 == China, 0x40 == Korea; others on DSi
	u8 rom_version;
	u8 autostart;
	struct {
		u32 rom_offset;
		u32 entry_address;
		u32 ram_address;
		u32 size;
	} arm9;
	struct {
		u32 rom_offset;
		u32 entry_address;
		u32 ram_address;
		u32 size;
	} arm7;

	u32 fnt_offset;		// File Name Table offset
	u32 fnt_size;		// File Name Table size
	u32 fat_offset;
	u32 fat_size;

	u32 arm9_overlay_offset;
	u32 arm9_overlay_size;
	u32 arm7_overlay_offset;
	u32 arm7_overlay_size;

	u32 normal_40001A4;	// Port 0x40001A4 setting for normal commands (usually 0x00586000)
	u32 key1_40001A4;	// Port 0x40001A4 setting for KEY1 commands (usually 0x001808F8)

	u32 icon_offset;
	u16 secure_area_checksum;	// CRC32 of 0x0020...0x7FFF
	u16 secure_area_delay;		// Delay, in 131 kHz units (0x051E=10ms, 0x0D7E=26ms)

	u32 arm9_auto_load_list_ram_address;
	u32 arm7_auto_load_list_ram_address;

	u64 secure_area_disable;

	u32 total_used_rom_size;
	u32 rom_header_size;		// Usually 0x4000
	u8 reserved3[0x38];
	u8 nintendo_logo[0x9C];		// GBA-style Nintendo logo
	u16 nintendo_logo_checksum;	// CRC16 of nintendo_logo[] (always 0xCF56)
	u16 header_checksum;		// CRC16 of 0x0000...0x015D

	struct {
		u32 rom_offset;
		u32 size;
		u32 ram_address;
	} debug;

	u8 reserved4[4];
	u8 reserved5[0x90];
} __attribute__((__packed__))
NTR_HEADER;

#endif//UNCART_HEADERS_H_

