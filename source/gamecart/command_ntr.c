// Copyright 2014 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

/**
 * References:
 * - http://problemkaputt.de/gbatek.htm#dscartridgesencryptionfirmware
 * - http://problemkaputt.de/gbatek.htm#dscartridgeprotocol
 * - https://www.3dbrew.org/wiki/Gamecards
 */

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
