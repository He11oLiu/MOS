#include <inc/lib.h>

#define BACKGROUND 0x00

struct interface interface;

void input_handler();
void display_info();

void umain(int argc, char **argv)
{
    int r;
    if ((r = init_palette("/bin/sysinfo.plt", frame)) < 0)
        printf("Open palette fail %e\n", r);
    interface_init(graph.scrnx, graph.scrny, graph.framebuffer, &interface);
    interface.titletype = TITLE_TYPE_TXT;
    strcpy(interface.title, "System information");
    interface.title_color = 0x5a;
    interface.title_textcolor = 0xff;
    interface.content_type = APP_NEEDBG;
    interface.content_color = BACKGROUND;
    draw_interface(&interface);
    if ((r = draw_bitmap("/bin/sysinfo.bmp", 100, 160, &interface)) < 0)
        printf("Open clock back fail %e\n", r);
    display_info();
    sys_updatescreen();
    input_handler();
}

void input_handler()
{
    unsigned char ch;
    ch = getchar();
    while (1)
    {
        switch (ch)
        {
        case KEY_ESC:
            exit();
        }
        ch = getchar();
    }
}

void display_info()
{
    int display_y = 300;
    int display_x = 160;
    int fontmeg = 2;
    int font_height = fontmeg * 17;
    char buf[30];
    struct sysinfo info;
    sys_getinfo(&info);
    draw_ascii(display_x, display_y, "Sys    : He11o_Liu's MOS version 0.1", 0xff, 0x00, fontmeg, &interface);

    display_y += font_height;
    draw_ascii(display_x, display_y, "Github : https://github.com/He11oLiu/JOS", 0xff, 0x00, fontmeg, &interface);
    display_y += font_height;
    draw_ascii(display_x, display_y, "Blog   : http://blog.csdn.net/he11o_liu", 0xff, 0x00, fontmeg, &interface);

    display_y += font_height;
    snprintf(buf, sizeof(buf), "CPUs   : %d CPUs online", info.ncpu);
    draw_ascii(display_x, display_y, buf, 0xff, 0x00, fontmeg, &interface);

    display_y += font_height;
    snprintf(buf, sizeof(buf), "Boot   : %d CPU is boot CPU", info.bootcpu);
    draw_ascii(display_x, display_y, buf, 0xff, 0x00, fontmeg, &interface);

    display_y += font_height;
    snprintf(buf, sizeof(buf), "Memory : %uK", info.totalmem);
    draw_ascii(display_x, display_y, buf, 0xff, 0x00, fontmeg, &interface);
}