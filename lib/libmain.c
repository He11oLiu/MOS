// Called from entry.S to get us going.
// entry.S already took care of defining envs, pages, uvpd, and uvpt.

#include <inc/lib.h>

#define SCRNX 1024
#define SCRNY 768

extern void umain(int argc, char **argv);

const volatile struct Env *thisenv;
const char *binaryname = "<unknown>";

void
libmain(int argc, char **argv)
{
	// set thisenv to point at our Env structure in envs[].
	thisenv = &envs[ENVX(sys_getenvid())];

	// save the name of the program so that panic() can use it
	if (argc > 0)
		binaryname = argv[0];
	
	// Could be pass with frambuffer from kernel
	graph.scrnx = SCRNX;
	graph.scrny = SCRNY;
	graph.framebuffer = (uint8_t *)FRAMEBUF;
	framebuffer = (uint8_t *)FRAMEBUF;

	// call user main routine
	umain(argc, argv);

	// exit gracefully
	exit();
}

