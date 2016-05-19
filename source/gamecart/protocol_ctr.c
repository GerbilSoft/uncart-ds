// Copyright 2014 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "protocol_ctr.h"
#include "command_ctr.h"

#include "protocol.h"
#include "delay.h"
#include "draw.h"
#include "aes.h"

static u32 rand1 = 0;
static u32 rand2 = 0;

void CTR_SetSecKey(u32 value) {
    REG_CTRCARDSECCNT |= ((value & 3) << 8) | 4;
    while (!(REG_CTRCARDSECCNT & 0x4000));
}

void CTR_SetSecSeed(const u32* seed, bool flag) {
    REG_CTRCARDSECSEED = BSWAP32(seed[3]);
    REG_CTRCARDSECSEED = BSWAP32(seed[2]);
    REG_CTRCARDSECSEED = BSWAP32(seed[1]);
    REG_CTRCARDSECSEED = BSWAP32(seed[0]);
    REG_CTRCARDSECCNT |= 0x8000;

    while (!(REG_CTRCARDSECCNT & 0x4000));

    if (flag) {
        (*(vu32*)0x1000400C) = 0x00000001; // Enable cart command encryption?
    }
}

void CTR_SendCommand(const u32 command[4], u32 pageSize, u32 blocks, u32 latency, void* buffer)
{
#ifdef VERBOSE_COMMANDS
    Debug("C> %08X %08X %08X %08X", command[0], command[1], command[2], command[3]);
#endif

    REG_CTRCARDCMD[0] = command[3];
    REG_CTRCARDCMD[1] = command[2];
    REG_CTRCARDCMD[2] = command[1];
    REG_CTRCARDCMD[3] = command[0];

    //Make sure this never happens
    if(blocks == 0) blocks = 1;

    pageSize -= pageSize & 3; // align to 4 byte
    u32 pageParam = CTRCARD_PAGESIZE_4K;
    u32 transferLength = 4096;
    // make zero read and 4 byte read a little special for timing optimization(and 512 too)
    switch(pageSize) {
        case 0:
            transferLength = 0;
            pageParam = CTRCARD_PAGESIZE_0;
            break;
        case 4:
            transferLength = 4;
            pageParam = CTRCARD_PAGESIZE_4;
            break;
        case 64:
            transferLength = 64;
            pageParam = CTRCARD_PAGESIZE_64;
            break;
        case 512:
            transferLength = 512;
            pageParam = CTRCARD_PAGESIZE_512;
            break;
        case 1024:
            transferLength = 1024;
            pageParam = CTRCARD_PAGESIZE_1K;
            break;
        case 2048:
            transferLength = 2048;
            pageParam = CTRCARD_PAGESIZE_2K;
            break;
        case 4096:
            transferLength = 4096;
            pageParam = CTRCARD_PAGESIZE_4K;
            break;
	default:
	    break; //Defaults already set
    }

    REG_CTRCARDBLKCNT = blocks - 1;
    transferLength *= blocks;

    // go
    REG_CTRCARDCNT = 0x10000000;
    REG_CTRCARDCNT = /*CTRKEY_PARAM | */CTRCARD_ACTIVATE | CTRCARD_nRESET | pageParam | latency;

    u8 * pbuf = (u8 *)buffer;
    u32 * pbuf32 = (u32 * )buffer;
    bool useBuf = ( NULL != pbuf );
    bool useBuf32 = (useBuf && (0 == (3 & ((u32)buffer))));

    u32 count = 0;
    u32 cardCtrl = REG_CTRCARDCNT;

    if(useBuf32)
    {
        while( (cardCtrl & CTRCARD_BUSY) && count < transferLength)
        {
            cardCtrl = REG_CTRCARDCNT;
            if( cardCtrl & CTRCARD_DATA_READY  ) {
                u32 data = REG_CTRCARDFIFO;
                *pbuf32++ = data;
                count += 4;
            }
        }
    }
    else if(useBuf)
    {
        while( (cardCtrl & CTRCARD_BUSY) && count < transferLength)
        {
            cardCtrl = REG_CTRCARDCNT;
            if( cardCtrl & CTRCARD_DATA_READY  ) {
                u32 data = REG_CTRCARDFIFO;
                pbuf[0] = (unsigned char) (data >>  0);
                pbuf[1] = (unsigned char) (data >>  8);
                pbuf[2] = (unsigned char) (data >> 16);
                pbuf[3] = (unsigned char) (data >> 24);
                pbuf += sizeof (unsigned int);
                count += 4;
            }
        }
    }
    else
    {
        while( (cardCtrl & CTRCARD_BUSY) && count < transferLength)
        {
            cardCtrl = REG_CTRCARDCNT;
            if( cardCtrl & CTRCARD_DATA_READY  ) {
                u32 data = REG_CTRCARDFIFO;
                (void)data;
                count += 4;
            }
        }
    }

    // if read is not finished, ds will not pull ROM CS to high, we pull it high manually
    if( count != transferLength ) {
        // MUST wait for next data ready,
        // if ds pull ROM CS to high during 4 byte data transfer, something will mess up
        // so we have to wait next data ready
        do { cardCtrl = REG_CTRCARDCNT; } while(!(cardCtrl & CTRCARD_DATA_READY));
        // and this tiny delay is necessary
        ioDelay(33);
        // pull ROM CS high
        REG_CTRCARDCNT = 0x10000000;
        REG_CTRCARDCNT = CTRKEY_PARAM | CTRCARD_ACTIVATE | CTRCARD_nRESET;
    }
    // wait rom cs high
    do { cardCtrl = REG_CTRCARDCNT; } while( cardCtrl & CTRCARD_BUSY );
    //lastCmd[0] = command[0];lastCmd[1] = command[1];

#ifdef VERBOSE_COMMANDS
    if (!useBuf) {
        Debug("C< NULL");
    } else if (!useBuf32) {
        Debug("C< non32");
    } else {
        u32* p = (u32*)buffer;
        int transferWords = count / 4;
        for (int i = 0; i < transferWords && i < 4*4; i += 4) {
            switch (transferWords - i) {
            case 0:
                break;
            case 1:
                Debug("C< %08X", p[i+0]);
                break;
            case 2:
                Debug("C< %08X %08X", p[i+0], p[i+1]);
                break;
            case 3:
                Debug("C< %08X %08X %08X", p[i+0], p[i+1], p[i+2]);
                break;
            default:
                Debug("C< %08X %08X %08X %08X", p[i+0], p[i+1], p[i+2], p[i+3]);
                break;
            }
        }
    }
#endif
}

static void AES_SetKeyControl(u32 a) {
    REG_AESKEYCNT = (REG_AESKEYCNT & 0xC0) | a | 0x80;
}

//returns 1 if MAC valid otherwise 0
static u8 card_aes(u32 *out, u32 *buff, size_t size) { // note size param ignored
    u8 tmp = REG_AESKEYCNT;
    REG_AESCNT = 0x10C00;    //flush r/w fifo macsize = 001

    (*(vu8*)0x10000008) |= 0x0C; //???

    REG_AESCNT |= 0x2800000;

    //const u8 is_dev_unit = *(vu8*)0x10010010;
    //if(is_dev_unit) //Dev unit
    const u8 is_dev_cart = (A0_Response&3)==3;
    if(is_dev_cart) //Dev unit
    {
        AES_SetKeyControl(0x11);
        REG_AESKEYFIFO = 0;
        REG_AESKEYFIFO = 0;
        REG_AESKEYFIFO = 0;
        REG_AESKEYFIFO = 0;
        REG_AESKEYSEL = 0x11;
    }
    else
    {
        AES_SetKeyControl(0x3B);
        REG_AESKEYYFIFO = buff[0];
        REG_AESKEYYFIFO = buff[1];
        REG_AESKEYYFIFO = buff[2];
        REG_AESKEYYFIFO = buff[3];
        REG_AESKEYSEL = 0x3B;
    }

    REG_AESCNT = 0x4000000;
    REG_AESCNT &= 0xFFF7FFFF;
    REG_AESCNT |= 0x2970000;
    REG_AESMAC[0] = buff[11];
    REG_AESMAC[1] = buff[10];
    REG_AESMAC[2] = buff[9];
    REG_AESMAC[3] = buff[8];
    REG_AESCNT |= 0x2800000;
    REG_AESCTR[0] = buff[14];
    REG_AESCTR[1] = buff[13];
    REG_AESCTR[2] = buff[12];
    REG_AESBLKCNT = 0x10000;

    u32 v11 = ((REG_AESCNT | 0x80000000) & 0xC7FFFFFF); //Start and clear mode (ccm decrypt)
    u32 v12 = v11 & 0xBFFFFFFF; //Disable Interrupt
    REG_AESCNT = ((((v12 | 0x3000) & 0xFD7F3FFF) | (5 << 23)) & 0xFEBFFFFF) | (5 << 22);

    //REG_AESCNT = 0x83D73C00;
    REG_AESWRFIFO = buff[4];
    REG_AESWRFIFO = buff[5];
    REG_AESWRFIFO = buff[6];
    REG_AESWRFIFO = buff[7];
    while (((REG_AESCNT >> 5) & 0x1F) <= 3);
    out[0] = REG_AESRDFIFO;
    out[1] = REG_AESRDFIFO;
    out[2] = REG_AESRDFIFO;
    out[3] = REG_AESRDFIFO;
    return ((REG_AESCNT >> 21) & 1);
}

void CTR_Secure_Init(u32 *buf, u32 *out)
{
    u8 mac_valid = card_aes(out, buf, 0x200);

//    if (!mac_valid)
//        ClearScreen(bottomScreen, RGB(255, 0, 0));

    ioDelay(0xF0000);

    CTR_SetSecKey(A0_Response);
    CTR_SetSecSeed(out, true);

    rand1 = 0x42434445;//*((vu32*)0x10011000);
    rand2 = 0x46474849;//*((vu32*)0x10011010);

    CTR_CmdSeed(rand1, rand2);

    out[3] = BSWAP32(rand2);
    out[2] = BSWAP32(rand1);
    CTR_SetSecSeed(out, false);

    u32 test = 0;
    const u32 A2_cmd[4] = { 0xA2000000, 0x00000000, rand1, rand2 };
    CTR_SendCommand(A2_cmd, 4, 1, 0x701002C, &test);

    u32 test2 = 0;
    const u32 A3_cmd[4] = { 0xA3000000, 0x00000000, rand1, rand2 };
    CTR_SendCommand(A3_cmd, 4, 1, 0x701002C, &test2);
    
    if(test==CartID && test2==A0_Response)
    {
        const u32 C5_cmd[4] = { 0xC5000000, 0x00000000, rand1, rand2 };
        CTR_SendCommand(C5_cmd, 0, 1, 0x100002C, NULL);
    }

    for (int i = 0; i < 5; ++i) {
        CTR_SendCommand(A2_cmd, 4, 1, 0x701002C, &test);
        ioDelay(0xF0000);
    }
}

void CTR_Dummy(void) {
    // Sends a dummy command to skip encrypted responses some problematic carts send.
    u32 test;
    const u32 A2_cmd[4] = { 0xA2000000, 0x00000000, rand1, rand2 };
    CTR_SendCommand(A2_cmd, 4, 1, 0x701002C, &test);
}
