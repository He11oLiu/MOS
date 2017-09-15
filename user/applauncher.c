#include <inc/lib.h>

#define LAUNCHER_PALETTE "/bin/launcher.plt"
struct interface interface;
struct launcher_content launcher;

void input_handler();
void change_sel(int from, int to);
void launch_app();

void umain(int argc, char **argv)
{
    interface_init(graph.scrnx, graph.scrny, graph.framebuffer, &interface);
    interface.titletype = TITLE_TYPE_IMG;
    strcpy(interface.title, "/bin/title.bmp");
    interface.title_color = 0x00;

    init_palette(LAUNCHER_PALETTE, frame);
    draw_title(&interface);

    // init launcher
    launcher.app_num = 4;
    launcher.app_sel = 0;
    launcher.background = 0x00;
    strcpy(launcher.icon[0], "/bin/cal.bmp");
    strcpy(launcher.app_bin[0], "/bin/calendar");

    strcpy(launcher.icon[1], "/bin/term.bmp");
    strcpy(launcher.app_bin[1], "/bin/term");

    strcpy(launcher.icon[2], "/bin/setting.bmp");
    strcpy(launcher.app_bin[2], "/bin/system");

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
        // printf("%#x\n", ch);
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
        case KEY_ENTER:
            launch_app();
            break;
        }
        // case
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

void refresh_interface()
{
    draw_interface(&interface);
    draw_launcher(&interface, &launcher);
    sys_updatescreen();
}

void launch_app()
{
    char *app_bin = launcher.app_bin[launcher.app_sel];
    int r;
    char *argv[2];
    argv[0] = app_bin;
    argv[1] = 0;
    printf("[launcher] Launching %s\n",app_bin);
    if ((r = spawn(app_bin, (const char **)argv)) < 0)
    {
        printf("App %s not found!\n",app_bin);
        return;
    }
    wait(r);
    printf("[launcher] %s normally exit\n",app_bin);
    init_palette(LAUNCHER_PALETTE, frame);
    refresh_interface();
}