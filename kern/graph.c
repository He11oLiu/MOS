#include <inc/types.h>
#include <inc/assert.h>
#include <inc/x86.h>
#include <inc/memlayout.h>
#include <inc/string.h>
#include <kern/graph.h>
#include <kern/pmap.h>

#define PIXEL(x, y) *(framebuffer + x + (y * graph.scrnx))
struct graph_info graph;
uint8_t *framebuffer;

// initial frambuffer
void init_framebuffer();
// update screen
void update_screen();

void graph_init()
{
    int i;
    // Init Graph MMIO
    graph.vram = (uint8_t *)mmio_map_region((physaddr_t)graph.vram,
                                            graph.scrnx * graph.scrny);

    cprintf("====Graph mode on====\n");
    cprintf("   scrnx  = %d\n", graph.scrnx);
    cprintf("   scrny  = %d\n", graph.scrny);
    cprintf("MMIO VRAM = %#x\n", graph.vram);
    cprintf("=====================\n");

    // Init framebuffer
    init_framebuffer();
    init_palette();
    draw_screen(0x03);
    update_screen();
}

int draw_canvas(uint16_t x, uint16_t y, canvas_t *canvas)
{
    int i, j;
    int width = (x + canvas->width) > graph.scrnx ? graph.scrnx : (x + canvas->width);
    int height = (y + canvas->height) > graph.scrny ? graph.scrny : (y + canvas->height);
    cprintf("width %d height %d\n", width, height);
    for (j = y; j < height; j++)
        for (i = x; i < width; i++)
            PIXEL(i, j) = *(canvas->data + (i - x) + (j - y) * canvas->width);
    update_screen();
    return 0;
}

int draw_screen(uint8_t color)
{
    int i;
    for (i = 0; i < graph.scrnx * graph.scrny; i++)
        *(framebuffer + i) = color;
    update_screen();
    return 0;
}

void init_framebuffer()
{
    if ((framebuffer = (uint8_t *)kmalloc((size_t)(graph.scrnx * graph.scrny))) == NULL)
        panic("Not enough memory for framebuffer!");
    map_framebuffer(framebuffer);
}

void update_screen()
{
    memcpy(graph.vram, framebuffer, graph.scrnx * graph.scrny);
}

void init_palette()
{
    int i;
    outb(0x03c8, 0);
    for (i = 0; i < 256; i++)
    {
        outb(0x03c9, (i & 0xe0) << 0 | 0xA);
        outb(0x03c9, (i & 0x1c) << 3 | 0xA);
        outb(0x03c9, (i & 0x03) << 5 | 0xA);
    }
}