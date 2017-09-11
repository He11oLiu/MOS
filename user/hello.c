// hello, world
#include <inc/lib.h>


void umain(int argc, char **argv)
{
	struct tm time;
	sys_gettime(&time);
	cprintf("I am environment %08x\n", thisenv->env_id);
	cprintf("System tiem %t\n", &time);
	cprintf("Try flush frame_buffer %#x\n",framebuffer);
	for (int i = 0; i < 0xc0000; i++)
		*(framebuffer+i) = 0xe2;
	sys_updatescreen();
}
