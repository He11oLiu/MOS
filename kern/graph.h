#ifndef JOS_KERN_GRAPH_H_
#define JOS_KERN_GRAPH_H_ 1

#include <inc/types.h>
#include <inc/canvas.h>


struct palette
{
    unsigned char rgb_blue;
    unsigned char rgb_green;
    unsigned char rgb_red;
    unsigned char rgb_reserved;
};

struct frame_info
{
    struct palette palette[256];
    uint8_t *framebuffer;
};

struct graph_info
{
    uint16_t scrnx, scrny;
    uint8_t *vram;
};

extern struct graph_info graph;

void graph_init();
int draw_screen(uint8_t color);
void init_palette();
int draw_canvas(uint16_t x, uint16_t y,canvas_t *canvas);
void update_screen();
void set_palette();

#endif