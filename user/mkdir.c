#include <inc/lib.h>

void umain(int argc, char **argv)
{
    int r;

    if (argc != 2)
    {
        printf("usage: mkdir directory\n");
        return;
    }
    if((r = mkdir(argv[1])) < 0)
        printf("%s error : %e\n",argv[0],r);
}