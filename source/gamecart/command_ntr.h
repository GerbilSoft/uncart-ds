// Copyright 2014 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common.h"
#include "headers.h"

void NTR_CmdReset(void);
u32 NTR_CmdGetCartId(void);
void NTR_CmdEnter16ByteMode(void);

/**
 * Read the header of an NTR/TWL cartridge.
 * @param buffer Header buffer. (Must be >= 0x200 bytes)
 */
void NTR_CmdReadHeader(void* buffer);

/**
 * Read the Secure Area. (0x4000-0x7FFF)
 * This switches into KEY2 encryption mode.
 * @param buffer	[out] Output buffer.
 * @param ntrHeader	[in]  NTR header.
 */
void NTR_ReadSecureArea(void *buffer, const NTR_HEADER *ntrHeader);
