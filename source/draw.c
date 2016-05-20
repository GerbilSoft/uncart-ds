// Copyright 2013 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "font.h"
#include "draw.h"

size_t current_y = 0;

void ClearScreen(unsigned char *screen, int color)
{
    int i;
    unsigned char *screenPos = screen;
    for (int i = (SCREEN_HEIGHT * SCREEN_WIDTH); i > 0; i--) {
        *(screenPos++) = color >> 16;  // B
        *(screenPos++) = color >> 8;   // G
        *(screenPos++) = color & 0xFF; // R
    }
}

void DrawCharacter(unsigned char *screen, int character, u32 x, u32 y, int color, int bgcolor)
{
    for (int yy = 0; yy < 8; yy++) {
        u32 xDisplacement = (x * BYTES_PER_PIXEL * SCREEN_WIDTH);
        u32 yDisplacement = ((SCREEN_WIDTH - (y + yy) - 1) * BYTES_PER_PIXEL);
        
	unsigned char *screenPos = screen + xDisplacement + yDisplacement;
        unsigned char charPos = font[(size_t)character * 8 + yy];
        for (int xx = 7; xx >= 0; xx--) {
            if ((charPos >> xx) & 1) {
                *(screenPos + 0) = color >> 16;  // B
                *(screenPos + 1) = color >> 8;   // G
                *(screenPos + 2) = color & 0xFF; // R
            } else {
                *(screenPos + 0) = bgcolor >> 16;  // B
                *(screenPos + 1) = bgcolor >> 8;   // G
                *(screenPos + 2) = bgcolor & 0xFF; // R
            }
            screenPos += BYTES_PER_PIXEL * SCREEN_WIDTH;
        }
    }
}

void DrawString(unsigned char *screen, const char *str, u32 x, u32 y, int color, int bgcolor)
{
    for (; *str != 0; str++) {
        DrawCharacter(screen, *str, x, y, color, bgcolor);
        x += 8;
    }
}

void DrawStringF(u32 x, u32 y, const char *format, ...)
{
    char str[256];
    va_list va;

    va_start(va, format);
    vsnprintf(str, sizeof(str), format, va);
    va_end(va);

    DrawString(TOP_SCREEN0, str, x, y, RGB(0, 0, 0), RGB(255, 255, 255));
    DrawString(TOP_SCREEN1, str, x, y, RGB(0, 0, 0), RGB(255, 255, 255));
}

void Debug(const char *format, ...)
{
    static const char spaces[] = "                                                 X";

    char str[51];
    va_list va;
    va_start(va, format);
    vsnprintf(str, sizeof(str), format, va);
    va_end(va);
    snprintf(str, sizeof(str), "%s%s", str, spaces);

    // TODO: Disable double-buffering?
    DrawString(TOP_SCREEN0, str, 0u, current_y, RGB(255, 0, 0), RGB(255, 255, 255));
    DrawString(TOP_SCREEN0, spaces, 0u, current_y + 10, RGB(255, 0, 0), RGB(255, 255, 255));
    DrawString(TOP_SCREEN1, str, 0u, current_y, RGB(255, 0, 0), RGB(255, 255, 255));
    DrawString(TOP_SCREEN1, spaces, 0u, current_y + 10, RGB(255, 0, 0), RGB(255, 255, 255));

    current_y += 10;
    if (current_y >= 240) {
        current_y = 0;
    }
}
