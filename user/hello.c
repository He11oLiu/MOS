// hello, world
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	struct tm time;
	sys_gettime(&time);
	cprintf("hello, world\n");
	cprintf("i am environment %08x\n", thisenv->env_id);
	cprintf("%#2d:%#2d:%#2d\n",time.tm_hour,time.tm_min,time.tm_sec);
}
