#include <inc/lib.h>

void umain(int argc, char **argv)
{
	printf("hello world\n");
	char *buf = readline("Input:");
	printf("%s",buf);
	getchar();
}

