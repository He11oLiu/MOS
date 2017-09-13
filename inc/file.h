#ifndef JOS_INC_FILE_H
#define JOS_INC_FILE_H

#include <inc/types.h>
#include <inc/fd.h>


/* File open modes */
#define O_RDONLY 0x0000  /* open for reading only */
#define O_WRONLY 0x0001  /* open for writing only */
#define O_RDWR 0x0002	/* open for reading and writing */
#define O_ACCMODE 0x0003 /* mask for above modes */

#define O_CREAT 0x0100 /* create if nonexistent */
#define O_TRUNC 0x0200 /* truncate to zero length */
#define O_EXCL 0x0400  /* error if already exists */
#define O_MKDIR 0x0800 /* create directory, not regular file */

int open(const char *path, int mode);
int ftruncate(int fd, off_t size);
int remove(const char *path);
int sync(void);


// user fd api

int close(int fd);
ssize_t read(int fd, void *buf, size_t nbytes);
ssize_t write(int fd, const void *buf, size_t nbytes);
int seek(int fd, off_t offset);
void close_all(void);
ssize_t readn(int fd, void *buf, size_t nbytes);
int dup(int oldfd, int newfd);
int fstat(int fd, struct Stat *statbuf);
int stat(const char *path, struct Stat *statbuf);


#endif