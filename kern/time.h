#include <inc/types.h>

#define BCD_TO_BIN(val) ((((val) >> 4) * 10) + ((val)&15))
#define TIMEZONE 8

struct tm
{
    int tm_sec;   /* seconds */
    int tm_min;   /* minutes */
    int tm_hour;  /* hours */
    int tm_mday;  /* day of the month */
    int tm_mon;   /* month */
    int tm_year;  /* year */
    int tm_wday;  /* day of the week */
};

int gettime(struct tm *);