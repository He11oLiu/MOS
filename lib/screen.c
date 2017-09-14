
#include <inc/string.h>
#include <inc/lib.h>
#include <inc/bprintf.h>
#include <inc/interface.h>

static ssize_t devscreen_read(struct Fd *, void *, size_t);
static ssize_t devscreen_write(struct Fd *, const void *, size_t);
static int devscreen_close(struct Fd *);
static int devscreen_stat(struct Fd *, struct Stat *);

struct Dev devscreen =
    {
        .dev_id = 's',
        .dev_name = "screen",
        .dev_read = devscreen_read,
        .dev_write = devscreen_write,
        .dev_close = devscreen_close,
        .dev_stat = devscreen_stat};

int isscreen(int fdnum)
{
    int r;
    struct Fd *fd;
    if ((r = fd_lookup(fdnum, &fd)) < 0)
        return r;
    return fd->fd_dev_id == devscreen.dev_id;
}

int openscreen(struct interface *interface)
{
    int r;
    struct Fd *fd;
    if ((r = fd_alloc(&fd)) < 0)
        return r;
    if ((r = sys_page_alloc(0, fd, PTE_P | PTE_U | PTE_W | PTE_SHARE)) < 0)
        return r;
    fd->fd_dev_id = devscreen.dev_id;
    fd->fd_omode = O_RDWR;
    set_screen_interface(interface);
    return fd2num(fd);
}

static ssize_t
devscreen_read(struct Fd *fd, void *vbuf, size_t n)
{
    int c;
    if (n == 0)
        return 0;
    while ((c = sys_cgetc()) == 0)
        sys_yield();
    if (c < 0)
        return c;
    if (c == 0x04)
        return 0;
    *(char *)vbuf = c;
    return 1;
}

static ssize_t
devscreen_write(struct Fd *fd, const void *vbuf, size_t n)
{
    int tot, m;
    char buf[128];
    // mistake: have to nul-terminate arg to sys_cputs,
    // so we have to copy vbuf into buf in chunks and nul-terminate.
    for (tot = 0; tot < n; tot += m)
    {
        m = n - tot;
        if (m > sizeof(buf) - 1)
            m = sizeof(buf) - 1;
        memmove(buf, (char *)vbuf + tot, m);
        bprintf("%.*s", m, buf);
    }
    return tot;
}

static int
devscreen_close(struct Fd *fd)
{
    USED(fd);
    return 0;
}

static int
devscreen_stat(struct Fd *fd, struct Stat *stat)
{
    strcpy(stat->st_name, "<screen>");
    return 0;
}
