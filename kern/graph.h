#ifndef JOS_KERN_GRAPH_H
#define JOS_KERN_GRAPH_H

struct graph_info
{
    short scrnx,scrny;
    char *vram;
};

extern struct graph_info graph;

void graph_init();

#endif