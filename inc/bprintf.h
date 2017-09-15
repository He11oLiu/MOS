#ifndef JOS_INC_BPRINTF_H
#define JOS_INC_BPRINTF_H

#include <inc/stdarg.h>
#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/usyscall.h>
#include <inc/string.h>

#define SCREEN_ROW 35
#define SCREEN_COL 103
#define SCREEN_SIZE (SCREEN_ROW * SCREEN_COL)

struct screen
{
    uint8_t screen_col;
    uint8_t screen_row;
    uint16_t screen_pos;
    char screen_buf[SCREEN_SIZE];
};

void bputchar(char c);
static void bputch(int ch, int *cnt);
int vbprintf(const char *fmt, va_list ap);
int bprintf(const char *fmt, ...);

#endif