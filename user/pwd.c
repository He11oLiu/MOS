#include <inc/lib.h>

void umain(int argc, char **argv)
{
    char path[200];
    if(argc > 1)
        printf("%s : too many arguments\n",argv[0]);
    else
        printf("%s\n",getcwd(path,200));
}