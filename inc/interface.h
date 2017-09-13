#ifndef JOS_INTERFACE_H
#define JOS_INTERFACE_H

#include <inc/types.h>
#include <inc/font.h>
#include <inc/file.h>

#define TITLE_HEIGHT 100

#define MAX_TITLE 30

#define PIXEL(interface, x, y) *(interface->framebuffer + x + (y * interface->scrnx))

struct interface
{
    char title[MAX_TITLE];
    uint8_t title_textcolor;
    uint8_t title_color;
    uint8_t content_color;

    // about the size and buff of interface
    uint16_t scrnx;
    uint16_t scrny;
    uint8_t *framebuffer;
};

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

void draw_interface(struct interface *interface);
void draw_title(struct interface *interface);
int draw_cn(uint16_t x, uint16_t y, char *str, uint8_t color,
            uint8_t fontmag, struct interface *interface);
int draw_ascii(uint16_t x, uint16_t y, char *str, uint8_t color,
               uint8_t fontmag, struct interface *interface);
void draw_fontpixel(uint16_t x, uint16_t y, uint8_t color,
                    uint8_t fontmag, struct interface *interface);
void interface_init(uint16_t scrnx, uint16_t scrny, uint8_t *framebuffer,
                    struct interface *interface);
void draw_content(struct interface *interface);
void add_title(char *title, uint8_t title_textcolor, uint8_t title_color,
               struct interface *interface);
void add_content(uint8_t content_color, struct interface *interface);
int init_palette(char *plt_filename, struct frame_info *frame);

#endif
