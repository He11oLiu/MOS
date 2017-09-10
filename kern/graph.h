#ifndef JOS_KERN_GRAPH_H_
#define JOS_KERN_GRAPH_H_ 1

#include <inc/types.h>
#include <inc/canvas.h>

struct graph_info
{
    uint16_t scrnx, scrny;
    uint8_t *vram;
};

extern struct graph_info graph;

void graph_init();
int draw_screen(uint8_t color);
// int draw_rect(uint16_t x, uint16_t y, uint16_t l, uint16_t w, uint8_t color);
// int draw_ascii(uint16_t x, uint16_t y, char *str, uint8_t color);
// int draw_cn(uint16_t x, uint16_t y, char *str, uint8_t color);
int draw_canvas(uint16_t x, uint16_t y,canvas_t *canvas);

#endif