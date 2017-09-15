#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	struct tm time;
    struct sysinfo info;
	sys_gettime(&time);
    printf("____________________________________________\n");
    printf("/\\                                           \\\n");
    printf("\\_| He11o_Liu's JOS version 0.1              |\n");
    printf("  | Github : https://github.com/He11oLiu/JOS |\n");
    printf("  | Blog   : http://blog.csdn.net/he11o_liu  |\n");
    printf("  |   _______________________________________|_\n");
    printf("   \\_/_________________________________________/\n");
    printf("\n");
    printf("System time : [%t]\n",&time);
    sys_getinfo(&info);
    printf("CPU number  : %d CPUs online\n", info.ncpu);
    printf("Boot CPU    : %d CPU is boot CPU\n", info.bootcpu);
    printf("Memory size : Physical memory %uK\n", info.totalmem);
}
