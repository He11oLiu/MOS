#include <inc/lib.h>

void umain(int argc, char **argv)
{
    int r;
    char *filename;
    char pathbuf[MAXPATH];
    pathbuf[0] = '\0';
    if (argc != 2)
    {
        printf("usage: touch filename\n");
        return;
    }
    filename = argv[1];
    if (*filename != '/')
        getcwd(pathbuf, MAXPATH);
    strcat(pathbuf, filename);
    if ((r = open(pathbuf, O_CREAT)) < 0)
        printf("%s error : %e\n", argv[0], r);
    close(r);
}