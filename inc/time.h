#ifndef JOS_INC_TIME_H
#define JOS_INC_TIME_H

#include <inc/types.h>

struct tm
{
    int tm_sec;  /* seconds */
    int tm_min;  /* minutes */
    int tm_hour; /* hours */
    int tm_mday; /* day of the month */
    int tm_mon;  /* month */
    int tm_year; /* year */
    int tm_wday; /* day of the week */
};

#endif