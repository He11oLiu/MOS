#include <inc/canvas.h>
#include <inc/font.h>
#include <inc/stdio.h>

#define CANVAS_PIXEL(canvas, x, y) *(canvas->data + x + (y * canvas->width))

int canvas_init(uint16_t width, uint16_t height, canvas_t *canvas)
{
    canvas->width = width;
    canvas->height = height;
    // malloc not implemented
    return 0;
}

int canvas_draw_bg(uint8_t color, canvas_t *canvas)
{
    int i;
    for (i = 0; i < canvas->width * canvas->height; i++)
        *(canvas->data + i) = color;
    return 0;
}

int canvas_draw_ascii(uint16_t x, uint16_t y, char *str, uint8_t color, canvas_t *canvas)
{
    // char *font;
    // int i, j, k = 0;
    // for (k = 0; str[k] != 0; k++)
    // {
    //     font = (char *)(ascii_8_16 + (str[k] - 0x20) * 16);
    //     for (i = 0; i < 16; i++)
    //         for (j = 0; j < 8; j++)
    //             if ((font[i] << j) & 0x80)
    //                 CANVAS_PIXEL(canvas, (x + j), (y + i)) = color;
    //     x += 8;
    // }
    return 0;
}

int canvas_draw_cn(uint16_t x, uint16_t y, char *str, uint8_t color, canvas_t *canvas)
{
    // uint16_t font;
    // int i, j, k;
    // int offset;
    // for (k = 0; str[k] != 0; k += 2)
    // {
    //     offset = ((char)(str[k] - 0xa0 - 1) * 94 +
    //               ((char)(str[k + 1] - 0xa0) - 1)) *
    //              32;
    //     for (i = 0; i < 16; i++)
    //     {
    //         font = cn_lib[offset + i * 2] << 8 |
    //                cn_lib[offset + i * 2 + 1];
    //         for (j = 0; j < 16; j++)
    //             if ((font << j) & 0x8000)
    //                 CANVAS_PIXEL(canvas, (x + j), (y + i)) = color;
    //     }
    //     x += 16;
    // }
    return 0;
}
int canvas_draw_rect(uint16_t x, uint16_t y, uint16_t l, uint16_t w, uint8_t color, canvas_t *canvas)
{
    int i, j;
    w = (y + w) > canvas->height ? canvas->height : (y + w);
    l = (x + l) > canvas->width ? canvas->width : (x + l);
    for (j = y; j < w; j++)
        for (i = x; i < l; i++)
            CANVAS_PIXEL(canvas, i, j) = color;
    return 0;
}