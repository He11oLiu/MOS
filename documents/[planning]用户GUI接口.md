# GUI

*本文暂时只是记录一下想法，不一定会选择实现*

在`图形库`中，已经将图形模式打开，将显存映射到内存中的一段空间。并进行了简单的测试。

实际上，直接对显存写是很不负责任的行为。很早之前在写`java`的界面的时候，就接触了双缓冲技术，其实与显示有关的思想都是差不多的，我们应该提供一个`framebuffer`。当完成一个`frame`后，再将这个`frame` update到显存中。

```c
uint8_t *framebuffer;
void init_framebuffer(){
  	if((framebuffer = (uint8_t *) kmalloc((size_t)(graph.scrnx*graph.scrny)))== NULL)
        panic("Not enough memory for framebuffer!");
}

void update_screen(){
    memcpy(graph.vram,framebuffer,graph.scrnx*graph.scrny);
}
```

经过实现`kmalloc`与`kfree`，已经可以分配这个缓冲区，并直接向缓冲区写入，最后再进行`update`

```c
#define PIXEL(x, y) *(framebuffer + x + (y * graph.scrnx))
int draw_xx()
{
	xxx;
	update_screen();
}
```



从一个单一的应用程序角度来看，应分配一个单独的画布，然后选择在一个位置显示。

```c
typedef struct canvas
{
    unsigned int width;
    unsigned int height;
    unsigned char *data;
} canvas_t;
```

