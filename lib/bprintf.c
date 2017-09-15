#include <inc/bprintf.h>
#include <inc/interface.h>

uint16_t screen_pos = 0;

struct screen screen;


static void bputch(int ch, int *cnt)
{
    bputchar(ch);
    sys_updatescreen();
    *cnt++;
}

int vbprintf(const char *fmt, va_list ap)
{
    int cnt = 0;
    vprintfmt((void *)bputch, &cnt, fmt, ap);
    return cnt;
}

int bprintf(const char *fmt, ...)
{
    va_list ap;
    int cnt;

    va_start(ap, fmt);
    cnt = vbprintf(fmt, ap);
    va_end(ap);
    return cnt;
}

void bputchar(char c)
{
    switch (c)
    {
    case '\b': /* backspace */
        if (screen_pos > 0)
        {
            screen_pos--;
            // delete the character
            screen.screen_buf[screen_pos] = ' ';
        }
        break;
    case '\n': /* new line */
        screen_pos += SCREEN_COL;
    /* fallthru */
    case '\r': /* return to the first character of cur line */
        screen_pos -= (screen_pos % SCREEN_COL);
        break;
    case '\t':
        bputchar(' ');
        bputchar(' ');
        bputchar(' ');
        bputchar(' ');
        break;
    default:
        screen.screen_buf[screen_pos++] = c; /* write the character */
        break;
    }

    // When current pos reach the bottom of the creen
    // case '\n' : screen_pos -= SCREEN_COL will work
    // case other: screen_pos must equal to SCREEN_SIZE
    if (screen_pos >= SCREEN_SIZE)
    {
        int i;
        // Move all the screen upward (a line)
        cprintf("before memmove!");
        memmove(screen.screen_buf, screen.screen_buf + SCREEN_COL, (SCREEN_SIZE - SCREEN_COL) * sizeof(uint8_t));
        cprintf("after memmove!");
        // Clear the bottom line
        for (i = SCREEN_SIZE - SCREEN_COL; i < SCREEN_SIZE; i++)
            screen.screen_buf[i] = ' ';
        screen_pos -= SCREEN_COL;
    }
    screen.screen_col = SCREEN_COL;
    screen.screen_row = SCREEN_ROW;
    draw_screen(100, 80, &screen, 0x00, 0xff, 1);
}
