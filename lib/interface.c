#include <inc/interface.h>
#include <inc/string.h>
#include <inc/error.h>
#include <inc/file.h>
#include <inc/fd.h>
#include <inc/usyscall.h>
#include <inc/bitmap.h>
#include <inc/stdio.h>

uint16_t iconloc[6][2] = {{30, 130}, {360, 130}, {690, 130}, {30, 450}, {360, 450}, {690, 450}};

struct interface *screen_interface;
struct interface default_interface;
void default_screen_interface();

void draw_interface(struct interface *interface)
{
    draw_title(interface);
    draw_content(interface);
}

void draw_title(struct interface *interface)
{
    int len, fontmag, titlex, titley;
    switch (interface->titletype)
    {
    case TITLE_TYPE_TXT:
        len = strlen(interface->title);
        fontmag = 3;
        titlex = (int)(interface->scrnx - len * 8 * fontmag) / 2 - 50;
        titley = (int)(TITLE_HEIGHT - 16 * fontmag) / 2;
        memset(interface->framebuffer, interface->title_color, TITLE_HEIGHT * interface->scrnx);
        draw_ascii(titlex, titley, interface->title, interface->title_textcolor, interface->title_textcolor, 4, interface);
        break;
    case TITLE_TYPE_IMG:
        memset(interface->framebuffer, interface->title_color, TITLE_HEIGHT * interface->scrnx);
        draw_bitmap(interface->title, (interface->scrnx - TITLE_IMG_WIDTH) / 2, 0, interface);
        break;
    }
}

void draw_launcher(struct interface *interface, struct launcher_content *launcher)
{
    int i;
    char buf[MAX_PATH];
    // memset(interface->framebuffer + TITLE_HEIGHT * interface->scrny, launcher->background, (interface->scrny - TITLE_HEIGHT) * interface->scrnx);
    memset(interface->framebuffer + TITLE_HEIGHT * interface->scrnx, launcher->background, (interface->scrny - TITLE_HEIGHT) * interface->scrnx);
    for (i = 0; i < launcher->app_num; i++)
    {
        if (i == launcher->app_sel)
        {
            strcpy(buf, launcher->icon[i]);
            strcpy(buf + strlen(launcher->icon[i]) - 4, "sel.bmp");
            draw_bitmap(buf, iconloc[i][0], iconloc[i][1], interface);
        }
        else
            draw_bitmap(launcher->icon[i], iconloc[i][0], iconloc[i][1], interface);
    }
}

void interface_init(uint16_t scrnx, uint16_t scrny, uint8_t *framebuffer, struct interface *interface)
{
    interface->scrnx = scrnx;
    interface->scrny = scrny;
    interface->framebuffer = framebuffer;
}

void add_title(char *title, uint8_t title_textcolor, uint8_t title_color, struct interface *interface)
{
    interface->titletype = TITLE_TYPE_TXT;
    strcpy(interface->title, title);
    interface->title_textcolor = title_textcolor;
    interface->title_color = title_color;
}

int draw_ascii(uint16_t x, uint16_t y, char *str, uint8_t color, uint8_t back, uint8_t fontmag, struct interface *interface)
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
                else if (color != back)
                    draw_fontpixel((x + j * fontmag), (y + i * fontmag), back, fontmag, interface);
        x += 8 * fontmag;
    }
    return k;
}

int draw_cn(uint16_t x, uint16_t y, char *str, uint8_t color, uint8_t back, uint8_t fontmag, struct interface *interface)
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
                else if (color != back)
                    draw_fontpixel((x + j * fontmag), (y + i * fontmag), back, fontmag, interface);
        }
        x += 16 * fontmag;
    }
    return k;
}

int draw_screen(uint16_t x, uint16_t y, struct screen *screen, uint8_t color, uint8_t back, uint8_t fontmag)
{
    char *font;
    uint16_t i, j, screen_x, screen_y, dis_x, dis_y;
    char *str = screen->screen_buf;
    char c;

    if (screen_interface == 0)
        default_screen_interface();
    dis_x = x;
    dis_y = y;
    for (screen_y = 0; screen_y < screen->screen_row; screen_y++)
    {
        x = dis_x;
        y = dis_y;
        for (screen_x = 0; screen_x < screen->screen_col; screen_x++)
        {
            c = *(str + screen_x + screen_y * screen->screen_col);
            font = (char *)(ascii_8_16 + (c - 0x20) * 16);
            for (i = 0; i < 16; i++)
                for (j = 0; j < 8; j++)
                    if ((font[i] << j) & 0x80)
                        draw_fontpixel((x + j * fontmag), (y + i * fontmag), color, fontmag, screen_interface);
                    else if (color != back)
                        draw_fontpixel((x + j * fontmag), (y + i * fontmag), back, fontmag, screen_interface);
            x += 8 * fontmag;
        }
        dis_y += 16 * fontmag;
    }
    return 0;
}

void draw_fontpixel(uint16_t x, uint16_t y, uint8_t color, uint8_t fontmag, struct interface *interface)
{
    int i, j;
    for (j = y; j < y + fontmag; j++)
        for (i = x; i < x + fontmag; i++)
            PIXEL(interface, i, j) = color;
}

int init_palette(char *plt_filename, struct frame_info *frame)
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

void draw_content(struct interface *interface)
{
    memset(interface->framebuffer + TITLE_HEIGHT * interface->scrnx, interface->content_color, (interface->scrny - TITLE_HEIGHT) * interface->scrnx);
}

void set_screen_interface(struct interface *screen)
{
    screen_interface = screen;
}

void default_screen_interface()
{
    default_interface.scrnx = (uint16_t)SCRNX;
    default_interface.scrny = (uint16_t)SCRNY;
    default_interface.framebuffer = (uint8_t *)FRAMEBUF;
    screen_interface = &default_interface;
}