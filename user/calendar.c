#include <inc/lib.h>

#define BACKGROUND 0x00

struct interface interface;
char buf[20];
int child_envid;

void display_time();
void input_handler();
void update_time();
char week[] = {0xcc, 0xec, 0xd2, 0xbb, 0xb6, 0xfe, 0xc8, 0xfd, 0xcb, 0xc4, 0xce, 0xe5, 0xc1, 0xf9};
char preweek[] = {0xd0, 0xc7, 0xc6, 0xda};
/* GB2312 */

void umain(int argc, char **argv)
{
    struct tm time;
    int r;
    if ((r = init_palette("/bin/clock.plt", frame)) < 0)
        printf("Open palette fail %e\n", r);
    interface_init(graph.scrnx, graph.scrny, graph.framebuffer, &interface);
    interface.titletype = TITLE_TYPE_TXT;
    strcpy(interface.title, "Calendar");
    interface.title_color = 0x34;
    interface.title_textcolor = 0xFF;
    interface.content_type = APP_NEEDBG;
    interface.content_color = BACKGROUND;
    draw_interface(&interface);
    if ((r = draw_bitmap("/bin/clock.bmp", 100, 300, &interface)) < 0)
        printf("Open clock back fail %e\n", r);
    sys_gettime(&time);
    display_time(&time);
    sys_updatescreen();

    if ((child_envid = fork()) == 0)
        update_time();
    input_handler();
}

void display_time(struct tm *time_ptr)
{

    strcpy(buf, preweek);
    *(buf + 4) = *(week + (time_ptr->tm_wday - 1) * 2);
    *(buf + 5) = *(week + (time_ptr->tm_wday - 1) * 2 + 1);
    *(buf + 6) = 0;
    draw_cn(640, 420, buf, 0x00, 0xff, 2, &interface);
    snprintf(buf, sizeof(buf), "%t", time_ptr);
    draw_ascii(300, 480, buf, 0xff, BACKGROUND, 4, &interface);
    snprintf(buf, sizeof(buf), "%d", time_ptr->tm_mday);
    draw_ascii(640, 460, buf, 0x00, 0xff, 6, &interface);
    if (time_ptr->tm_mon < 10)
        snprintf(buf, sizeof(buf), "20%d-0%d", time_ptr->tm_year, time_ptr->tm_mon);
    else
        snprintf(buf, sizeof(buf), "20%d-%d", time_ptr->tm_year, time_ptr->tm_mon);
    draw_ascii(630, 560, buf, 0x00, 0xff, 2, &interface);
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
            sys_env_destroy(child_envid);
            exit();
        }
        // case
        sys_yield();
        ch = getchar();
    }
}

void update_time()
{
    struct tm time;
    int oldsec = -1;
    while (1)
    {
        sys_gettime(&time);
        if (time.tm_sec != oldsec)
        {
            oldsec = time.tm_sec;
            display_time(&time);
            sys_updatescreen();
        }
    }
}