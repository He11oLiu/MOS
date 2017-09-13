#include <inc/lib.h>

#define KEY_UP 0xe2
#define KEY_DOWN 0xe3
#define KEY_LEFT 0xe4
#define KEY_RIGHT 0xe5

struct interface interface;
struct launcher_content launcher;

void input_handler();
void change_sel(int from, int to);

void umain(int argc, char **argv)
{
    interface_init(graph.scrnx, graph.scrny, graph.framebuffer, &interface);
    interface.titletype = TITLE_TYPE_IMG;
    strcpy(interface.title, "/bin/title.bmp");
    interface.title_color = 0x00;

    init_palette("/bin/palette.plt", frame);
    draw_interface(&interface);
    launcher.app_num = 4;
    launcher.app_sel = 0;
    launcher.background = 0x00;
    strcpy(launcher.icon[0], "/bin/cal.bmp");
    strcpy(launcher.icon[1], "/bin/term.bmp");
    strcpy(launcher.icon[2], "/bin/setting.bmp");
    strcpy(launcher.icon[3], "/bin/drive.bmp");
    draw_launcher(&interface, &launcher);
    sys_updatescreen();

    input_handler();
}

void input_handler()
{
    unsigned char ch;
    int index = 0;
    int cursorloc = 0;
    ch = getchar();
    while (1)
    {
        ch = ch & 0xFF;
        printf("%#x\n",ch);
        switch (ch)
        {
        case KEY_UP:
            if (launcher.app_sel > 2)
            {
                change_sel(launcher.app_sel, launcher.app_sel - 3);
                launcher.app_sel -= 3;
                sys_updatescreen();
            }
            break;
        case KEY_DOWN:
            if (launcher.app_sel < 3 && (launcher.app_sel + 3) < launcher.app_num)
            {
                change_sel(launcher.app_sel, launcher.app_sel + 3);
                launcher.app_sel += 3;
                sys_updatescreen();
            }
            break;
        case KEY_LEFT:
            if (launcher.app_sel > 0)
            {
                change_sel(launcher.app_sel, launcher.app_sel - 1);
                launcher.app_sel -= 1;
                sys_updatescreen();
            }
            break;
        case KEY_RIGHT:
            if (launcher.app_sel < launcher.app_num - 1)
            {
                change_sel(launcher.app_sel, launcher.app_sel + 1);
                launcher.app_sel += 1;
                sys_updatescreen();
            }
            break;
        }
        ch = getchar();
    }
}

void change_sel(int from, int to)
{
    char buf[MAX_PATH];
    strcpy(buf, launcher.icon[to]);
    strcpy(buf + strlen(launcher.icon[to]) - 4, "sel.bmp");
    draw_bitmap(buf, iconloc[to][0], iconloc[to][1], &interface);
    draw_bitmap(launcher.icon[from], iconloc[from][0], iconloc[from][1], &interface);
}