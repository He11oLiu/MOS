#include <inc/types.h>
#include <inc/assert.h>
#include <inc/x86.h>
#include <inc/memlayout.h>
#include <inc/string.h>
#include <kern/cpu.h>
#include <kern/prwlock.h>
#include <kern/sched.h>
#include <kern/graph.h>
#include <kern/pmap.h>
#include <inc/font_ascii.h>
#include <inc/font_cn.h>

#define PIXEL(x, y) *(framebuffer + x + (y * graph.scrnx))
struct graph_info graph;
uint8_t *framebuffer;
// as we haven't implemented malloc
// uint8_t tmpbuf[1024*768];

// initial frambuffer
void init_framebuffer();
// update screen
void update_screen();

void graph_init()
{
    int i;
    char test_ascii[] = "Draw ascii test : Hello Liu!";
    char test_cn[] = "你好世界！";
    // Init Graph MMIO
    graph.vram = (uint8_t *)mmio_map_region((physaddr_t)graph.vram,
                                            graph.scrnx * graph.scrny);

    cprintf("====Graph mode on====\n");
    cprintf("   scrnx = %d\n", graph.scrnx);
    cprintf("   scrny = %d\n", graph.scrny);
    cprintf("MMIO VRAM = %#x\n", graph.vram);
    cprintf("=====================\n");

    // Init framebuffer
    init_framebuffer();

    // Draw Screen
    draw_screen(0xe2);
    draw_rect(20, 20, 350, 150, 0x22);
    draw_ascii(50, 50, test_ascii, 0xff);
    draw_cn(50, 100, test_cn, 0xff);
}

int draw_screen(uint8_t color)
{
    int i;
    for (i = 0; i < graph.scrnx * graph.scrny; i++)
        *(framebuffer + i) = color;
    update_screen();
    return 0;
}

int draw_rect(short x, short y, short l, short w, uint8_t color)
{
    int i, j;
    w = (y + w) > graph.scrny ? graph.scrny : (y + w);
    l = (x + l) > graph.scrnx ? graph.scrnx : (x + l);
    for (j = y; j < w; j++)
        for (i = x; i < l; i++)
            PIXEL(i,j) = color;
    update_screen();
    return 0;
}

int draw_ascii(short x, short y, char *str, uint8_t color)
{
    char *font;
    int i, j, k = 0;
    for (k = 0; str[k] != 0; k++)
    {
        font = (char *)(ascii_8_16 + (str[k] - 0x20) * 16);
        for (i = 0; i < 16; i++)
            for (j = 0; j < 8; j++)
                if ((font[i] << j) & 0x80)
                    PIXEL((x + j), (y + i)) = color;
        x += 8;
    }
    update_screen();
    return k;
}

int draw_cn(short x, short y, char *str, uint8_t color)
{
    uint16_t font;
    int i, j, k;
    int offset;
    for (k = 0; str[k] != 0; k += 2)
    {
        offset = ((char)(str[k] - 0xa0 - 1) * 94 +
                  ((char)(str[k + 1] - 0xa0) - 1)) *
                 32;
        for (i = 0; i < 16; i++)
        {
            font = cn_lib[offset + i * 2] << 8 |
                   cn_lib[offset + i * 2 + 1];
            for (j = 0; j < 16; j++)
                if ((font << j) & 0x8000)
                    PIXEL((x + j), (y + i)) = color;
        }
        x += 16;
    }
    update_screen();
    return 0;
}

void init_framebuffer(){
    // void *malloc_free_test;
    if((framebuffer = (uint8_t *) kmalloc((size_t)(graph.scrnx*graph.scrny)))== NULL)
        panic("Not enough memory for framebuffer!");
    // malloc_free_test = framebuffer;
    // kfree(framebuffer,(size_t)(graph.scrnx*graph.scrny));    
    // if((framebuffer = (uint8_t *) kmalloc((size_t)(graph.scrnx*graph.scrny)))== NULL)
    //     panic("kmalloc error!");
    // if(malloc_free_test == framebuffer)
    //     cprintf("kmalloc/kfree check success\n");
    // else
    //     panic("kmalloc/kfree error!\n");

    // framebuffer = tmpbuf;
    // if(framebuffer == NULL)
    //     panic("Not enough memory for framebuffer!");
}

void update_screen(){
    memcpy(graph.vram,framebuffer,graph.scrnx*graph.scrny);
}