
#include <inc/lib.h>
#include <inc/fd.h>

void umain(int argc, char **argv)
{
	int i, r;
	struct Fd *fd;
	struct Dev *dev;

	// Spin for a bit to let the console quiet
	for (i = 0; i < 10; ++i)
		sys_yield();

	r = fd_lookup(0,&fd);
	r = dev_lookup(fd->fd_dev_id,&dev);
	cprintf("ret %d fd0 dev %d mode %d devname %s\n",r,fd->fd_dev_id,fd->fd_omode,dev->dev_name);


	r = fd_lookup(1,&fd);
	r = dev_lookup(fd->fd_dev_id,&dev);
	cprintf("ret %d fd0 dev %d mode %d devname %s\n",r,fd->fd_dev_id,fd->fd_omode,dev->dev_name);
	
	close(0);
	// close(1);
	if ((r = opencons()) < 0)
		panic("opencons: %e", r);
	if (r != 0)
		panic("first opencons used fd %d", r);
	if ((r = dup(0, 1)) < 0)
		panic("dup: %e", r);

	for (;;)
	{
		char *buf;

		buf = readline("Type a line: ");
		if (buf != NULL)
			fprintf(1, "%s\n", buf);
		else
			fprintf(1, "(end of file received)\n");
	}
}
