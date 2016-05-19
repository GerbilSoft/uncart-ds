// Copyright 2014 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "protocol.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "draw.h"
#include "common.h"
#include "protocol_ctr.h"
#include "protocol_ntr.h"
#include "command_ctr.h"
#include "command_ntr.h"
#include "delay.h"

extern u8* bottomScreen;

u32 CartID = 0xFFFFFFFFu;

// TODO: Move to protocol_ctr.c.
//static u32 A0_Response = 0xFFFFFFFFu;
u32 A0_Response = 0xFFFFFFFFu;

u32 BSWAP32(u32 val) {
    return (((val >> 24) & 0xFF)) |
           (((val >> 16) & 0xFF) << 8) |
           (((val >> 8) & 0xFF) << 16) |
           ((val & 0xFF) << 24);
}

// TODO: Verify
static void ResetCartSlot(void)
{
    REG_CARDCONF2 = 0x0C;
    REG_CARDCONF &= ~3;

    if (REG_CARDCONF2 == 0xC) {
        while (REG_CARDCONF2 != 0);
    }

    if (REG_CARDCONF2 != 0)
        return;

    REG_CARDCONF2 = 0x4;
    while(REG_CARDCONF2 != 0x4);

    REG_CARDCONF2 = 0x8;
    while(REG_CARDCONF2 != 0x8);
}

static void SwitchToNTRCARD(void)
{
    REG_NTRCARDROMCNT = 0x20000000;
    REG_CARDCONF &= ~3;
    REG_CARDCONF &= ~0x100;
    REG_NTRCARDMCNT = NTRCARD_CR1_ENABLE;
}

static void SwitchToCTRCARD(void)
{
    REG_CTRCARDCNT = 0x10000000;
    REG_CARDCONF = (REG_CARDCONF & ~3) | 2;
}

#if 0
// FIXME: Should be a more general IsInserted() function...
int Cart_IsInserted(void)
{
    return (0x9000E2C2 == CTR_CmdGetSecureId(rand1, rand2) );
}
#endif

u32 Cart_GetID(void)
{
    return CartID;
}

void Cart_Init(void)
{
    ResetCartSlot(); //Seems to reset the cart slot?

    REG_CTRCARDSECCNT &= 0xFFFFFFFB;
    ioDelay(0x30000);

    SwitchToNTRCARD();
    ioDelay(0x30000);

    REG_NTRCARDROMCNT = 0;
    REG_NTRCARDMCNT &= 0xFF;
    ioDelay(0x40000);

    REG_NTRCARDMCNT |= (NTRCARD_CR1_ENABLE | NTRCARD_CR1_IRQ);
    REG_NTRCARDROMCNT = NTRCARD_nRESET | NTRCARD_SEC_SEED;
    while (REG_NTRCARDROMCNT & NTRCARD_BUSY);

    // Reset
    NTR_CmdReset();
    CartID = NTR_CmdGetCartId();

    // 3ds
    if (CartID & 0x10000000) {
        u32 unknowna0_cmd[2] = { 0xA0000000, 0x00000000 };
        NTR_SendCommand(unknowna0_cmd, 0x4, 0, &A0_Response);

        NTR_CmdEnter16ByteMode();
        SwitchToCTRCARD();
        ioDelay(0xF000);

        REG_CTRCARDBLKCNT = 0;
    }
}
