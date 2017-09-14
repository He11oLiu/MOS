#include <inc/types.h>
#include <inc/bitmap.h>
#include <inc/error.h>
#include <inc/file.h>
#include <inc/string.h>
#include <inc/stdio.h>

void colorCompression(int colorbit, uint16_t len, uint32_t *buf)
{
    uint8_t *buf_ptr = (uint8_t *)buf;
    uint8_t *color_ptr;
    uint8_t color;
    int i;
    switch (colorbit)
    {
    case 1:
        return;
    case 2: // 16 bit color not support
        return;
    case 3:
        for (i = 0; i < len; i++)
        {
            color_ptr = (uint8_t *)(buf + i);
            color = *(color_ptr);
            *(buf_ptr + i) |= color & 0xE0;
            color_ptr++;
            color = *(color_ptr);
            *(buf_ptr + i) |= (color >> 3) & 0x1C;
            color_ptr++;
            color = *(color_ptr);
            *(buf_ptr + i) |= (color >> 6) & 0x3;
        }
        break;
    case 4:
        for (i = 0; i < len; i++)
        {
            color_ptr = (uint8_t *)(buf + i);
            color_ptr++;
            color = *(color_ptr);
            *(buf_ptr + i) |= color & 0xE0;
            color_ptr++;
            color = *(color_ptr);
            *(buf_ptr + i) |= (color >> 3) & 0x1C;
            color_ptr++;
            color = *(color_ptr);
            *(buf_ptr + i) |= (color >> 6) & 0x3;
        }
        break;
    default:
        return;
    }
}

int draw_bitmap(char *filename, uint16_t disx, uint16_t disy, struct interface *interface)
{
    int fd, i, j;
    int colorbit = 0;
    off_t offset;
    uint32_t buf[500];
    uint16_t width;
    int height;
    bitmap_fileheader head;
    bitmap_infoheader info;

    if ((fd = open(filename, O_RDONLY)) < 0)
        return -E_BAD_PATH;
    read(fd, &head, sizeof(bitmap_fileheader));
    read(fd, &info, sizeof(bitmap_infoheader));
    width = info.biWidth;
    height = info.biHeight;
    offset = head.bfOffBits;
    colorbit = info.biBitCount;
    if (colorbit != 8)
        return -E_INVAL;
    if (disx + width > interface->scrnx ||
        disy + height > interface->scrny)
        return -E_INVAL;
    seek(fd, offset);
    if (height > 0)
    {
        for (j = 0; j < height; j++)
        {
            read(fd, (void *)buf, width * (colorbit / 8));
            memcpy(interface->framebuffer + disx + (disy + height - j - 1) * interface->scrnx, buf, width);
        }
    }
    else
    {
        height = -height;
        for (j = 0; j < height; j++)
        {
            read(fd, (void *)buf, width * colorbit);
            memcpy(interface->framebuffer + disx + (disy + j) * interface->scrnx, buf, width);
        }
    }
    // printf("Width = %d Height = %d\n", width, height);
    // printf("Read finish\n");
    close(fd);
    return 0;
}
