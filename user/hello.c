// hello, world
#include <inc/lib.h>

// void init_palette()
// {
// 	int fd = open("/bin/palette.plt", O_RDONLY);
// 	int i;
// 	if (fd < 0)
// 		return;
// 	for (i = 0; i < 256; i++)
// 		read(fd, &frame->palette[i], sizeof(struct palette));
// 	close(fd);
// 	sys_setpalette();
// }

void umain(int argc, char **argv)
{
	struct tm time;
	sys_gettime(&time);
	int fd = 0;
	if (argc != 2)
		fd = open("/bin/rocket.bmp", O_RDONLY);
	else
		fd = open(argv[1], O_RDONLY);
	int i, j;
	off_t offset;
	uint8_t buf[1000];

	bitmap_fileheader head;
	bitmap_infoheader info;
	uint16_t width, height;
	bitmap_image image;

	bitmap_infoheader *infoheader = &info;
	read(fd, &head, sizeof(bitmap_fileheader));
	read(fd, &info, sizeof(bitmap_infoheader));

	width = infoheader->biWidth;
	height = infoheader->biHeight;
	offset = head.bfOffBits;
	printf("width   = %u\n", infoheader->biWidth);
	printf("height  = %u\n", infoheader->biHeight);
	printf("palette = %d\n", infoheader->biClrUsed);

	seek(fd, offset);
	read(fd, (void *)buf, width * height);
	uint16_t x = 20;
	uint16_t y = 20;
	for (j = 0; j < height; j++)
		for (i = 0; i < width; i++)
		{
			*(framebuffer + x + i + (y + j) * 1024) = *(buf + i + (height - j - 1) * width);
		}
	sys_updatescreen();
}

