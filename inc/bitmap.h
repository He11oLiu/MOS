#ifndef JOS_INC_BITMAP_H_
#define JOS_INC_BITMAP_H_

#include <inc/types.h>


typedef struct bitmap_fileheader
{
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
}__attribute__((packed)) bitmap_fileheader;

typedef struct bitmap_infoheader
{
    uint32_t biSize;
    uint32_t biWidth;
    uint32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    uint32_t biXPelsPerMeter;
    uint32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} bitmap_infoheader;

typedef struct
{
    uint16_t width;
    uint16_t height;
    int channel;
} bitmap_image;

#endif