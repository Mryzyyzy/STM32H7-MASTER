/**
 * ===========================================================================
 * BSP - LCD Font Library
 * ===========================================================================
 *
 * ASCII fonts: 32*16, 24*12, 20*10, 16*08, 12*06
 * Chinese fonts: 12*12, 16*16, 20*20, 24*24, 32*32
 *
 * Font generation tool: PCtoLCD2018, C51 format.
 * Each Chinese font table includes a 2-byte GB2312 key as the first entry.
 */
#pragma once
#include <stdint.h>

typedef struct _pFont {
    const uint8_t *pTable;      /* Font bitmap data pointer */
    uint16_t       Width;       /* Character width (pixels) */
    uint16_t       Height;      /* Character height (pixels) */
    uint16_t       Sizes;       /* Bytes per character */
    uint16_t       Table_Rows;  /* Total entries (Chinese fonts only) */
} pFONT;

/* Chinese fonts */
extern pFONT CN_Font12;
extern pFONT CN_Font16;
extern pFONT CN_Font20;
extern pFONT CN_Font24;
extern pFONT CN_Font32;

/* ASCII fonts */
extern pFONT ASCII_Font32;
extern pFONT ASCII_Font24;
extern pFONT ASCII_Font20;
extern pFONT ASCII_Font16;
extern pFONT ASCII_Font12;
