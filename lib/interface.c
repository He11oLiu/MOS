#include <inc/interface.h>
#include <inc/string.h>
#include <inc/error.h>
#include <inc/file.h>
#include <inc/fd.h>
#include <inc/usyscall.h>

void draw_interface(struct interface *interface)
{
    draw_title(interface);
    draw_content(interface);
    sys_updatescreen();
}

void draw_title(struct interface *interface)
{
    int len = strlen(interface->title);
    int fontmag = 3;
    int titlex = (int)(interface->scrnx - len * fontmag * 8) / 2;
    int titley = (int)(TITLE_HEIGHT - 16 * fontmag) / 2;
    memset(interface->framebuffer, interface->title_color, TITLE_HEIGHT * interface->scrnx);
    draw_ascii(titlex, titley, interface->title, interface->title_textcolor, 3, interface);
}

void draw_content(struct interface *interface)
{
    memset(interface->framebuffer + TITLE_HEIGHT * interface->scrnx, interface->content_color, (interface->scrny - TITLE_HEIGHT) * interface->scrnx);
}

void interface_init(uint16_t scrnx, uint16_t scrny, uint8_t *framebuffer, struct interface *interface)
{
    interface->scrnx = scrnx;
    interface->scrny = scrny;
    interface->framebuffer = framebuffer;
}

void add_title(char *title, uint8_t title_textcolor, uint8_t title_color, struct interface *interface)
{
    strcpy(interface->title, title);
    interface->title_textcolor = title_textcolor;
    interface->title_color = title_color;
}

void add_content(uint8_t content_color, struct interface *interface)
{
    interface->content_color = content_color;
}

int draw_ascii(uint16_t x, uint16_t y, char *str, uint8_t color, uint8_t fontmag, struct interface *interface)
{
    char *font;
    int i, j, k = 0;
    for (k = 0; str[k] != 0; k++)
    {
        font = (char *)(ascii_8_16 + (str[k] - 0x20) * 16);
        for (i = 0; i < 16; i++)
            for (j = 0; j < 8; j++)
                if ((font[i] << j) & 0x80)
                    draw_fontpixel((x + j * fontmag), (y + i * fontmag), color, fontmag, interface);
        x += 8 * fontmag;
    }
    return k;
}

int draw_cn(uint16_t x, uint16_t y, char *str, uint8_t color, uint8_t fontmag, struct interface *interface)
{
    uint16_t font;
    int i, j, k;
    int offset;
    for (k = 0; str[k] != 0; k += 2)
    {
        offset = ((char)(str[k] - 0xa0 - 1) * 94 +
                  ((char)(str[k + 1] - 0xa0) - 1)) *
                 32;
        for (i = 0; i < 16; i++)
        {
            font = cn_lib[offset + i * 2] << 8 |
                   cn_lib[offset + i * 2 + 1];
            for (j = 0; j < 16; j++)
                if ((font << j) & 0x8000)
                    draw_fontpixel((x + j * fontmag), (y + i * fontmag), color, fontmag, interface);
        }
        x += 16 * fontmag;
    }
    return k;
}

void draw_fontpixel(uint16_t x, uint16_t y, uint8_t color, uint8_t fontmag, struct interface *interface)
{
    int i, j;
    for (j = y; j < y + fontmag; j++)
        for (i = x; i < x + fontmag; i++)
            PIXEL(interface, i, j) = color;
}

int init_palette(char *plt_filename,struct frame_info *frame)
{
    int fd, i;
    if ((fd = open(plt_filename, O_RDONLY)) < 0)
        return -E_BAD_PATH;
    for (i = 0; i < 256; i++)
        read(fd, &frame->palette[i], sizeof(struct palette));
    close(fd);
    sys_setpalette();
    return 0;
}