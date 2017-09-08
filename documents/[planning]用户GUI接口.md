# GUI

*本文暂时只是记录一下想法，不一定会选择实现*

在`图形库`中，已经将图形模式打开，将显存映射到内存中的一段空间。并进行了简单的测试。

实际上，直接对显存写是很不负责任的行为。很早之前在写`java`的界面的时候，就接触了双缓冲技术，其实与显示有关的思想都是差不多的，我们应该提供一个`framebuffer`。当完成一个`frame`后，再将这个`frame` update到显存中。

```c
uint8_t *framebuffer;
void init_framebuffer(){
  	framebuffer = (uint32_t) malloc(graph_info->scrnx*graph_info->scrny);
}

void update_screen(){
    memcpy(graph_info->vram,framebuffer,graph_info->scrnx*graph_info->scrny);
}
```

而从一个单一的应用程序角度来看，应分配一个单独的画布，然后选择在一个位置显示。

```c
typedef struct canvas
{
    unsigned int width;
    unsigned int height;
    unsigned char *data;
} canvas_t;
```

