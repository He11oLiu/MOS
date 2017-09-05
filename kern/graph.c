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

struct graph_info graph;

void graph_init()
{
    int i;
    // Init Graph MMIO
    graph.vram =
        (char *)mmio_map_region((physaddr_t)graph.vram,
                                graph.scrnx * graph.scrny);
    cprintf("====Graph mode on====\n");
    cprintf("   scrnx = %d\n",graph.scrnx);
    cprintf("   scrny = %d\n",graph.scrny);
    cprintf("MMIO VRAM = %#x\n",graph.vram);
    cprintf("=====================\n");
    // Draw Screen
    for (i = 0; i < graph.scrnx * graph.scrny; i++)
        *(graph.vram + i) = 0x34;
}