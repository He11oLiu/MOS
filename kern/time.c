#include <inc/types.h>
#include <kern/time.h>
#include <kern/kclock.h>

int gettime(struct tm *tm)
{
    unsigned datas, datam, datah;
    int i;
    tm->tm_sec = BCD_TO_BIN(mc146818_read(0));
    tm->tm_min = BCD_TO_BIN(mc146818_read(2));
    tm->tm_hour = BCD_TO_BIN(mc146818_read(4)) + TIMEZONE;
    tm->tm_wday = BCD_TO_BIN(mc146818_read(6));
    tm->tm_mday = BCD_TO_BIN(mc146818_read(7));
    tm->tm_mon = BCD_TO_BIN(mc146818_read(8));
    tm->tm_year = BCD_TO_BIN(mc146818_read(9));
    return 0;
}