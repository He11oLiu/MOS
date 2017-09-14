#ifndef JOS_INC_SYSINFO_H
#define JOS_INC_SYSINFO_H

#include <inc/types.h>

struct sysinfo
{
    uint8_t ncpu;
    uint8_t bootcpu;
    ssize_t totalmem;
};

#endif