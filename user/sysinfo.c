#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	struct tm time;
    struct sysinfo info;
	sys_gettime(&time);
    printf("____________________________________________\n");
    printf("/\\                                           \\\n");
    printf("\\_| He11o_Liu's MOS version 0.1              |\n");
    printf("  | A HOEEMADE MicroOS based on MIT6.828 JOS |\n");
    printf("  | Github: https://github.com/He11oLiu/MOS  |\n");
    printf("  | Blog  : http://blog.csdn.net/he11o_liu   |\n");
    printf("  | Blog  : http://www.cnblogs.com/he11o-liu |\n");
    printf("  |   _______________________________________|_\n");
    printf("   \\_/_________________________________________/\n");
    printf("\n");
    printf("System time : [%t]\n",&time);
    sys_getinfo(&info);
    printf("CPU number  : %d CPUs online\n", info.ncpu);
    printf("Boot CPU    : %d CPU is boot CPU\n", info.bootcpu);
    printf("Memory size : Physical memory %uK\n", info.totalmem);
}
