#ifndef JOS_KERN_GRAPH_H
#define JOS_KERN_GRAPH_H

#include <inc/types.h>

struct graph_info
{
    short scrnx, scrny;
    uint8_t *vram;
};

typedef struct canvas
{
    unsigned int width;
    unsigned int height;
    uint8_t *data;
} canvas_t;

extern struct graph_info graph;

void graph_init();
int draw_screen(uint8_t color);
int draw_rect(short x, short y, short l, short w, uint8_t color);
int draw_ascii(short x, short y, char *str, uint8_t color);
int draw_cn(short x, short y, char *str, uint8_t color);
#endif