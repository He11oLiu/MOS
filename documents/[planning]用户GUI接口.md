# GUI

*本文暂时只是记录一下想法，不一定会选择实现*

## framebuffer

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



## canvas

从一个单一的应用程序角度来看，应分配一个单独的画布，然后选择在一个位置显示。

```c
typedef struct canvas
{
    uint16_t width;
    uint16_t height;
    uint8_t *data;
} canvas_t;
```

设计的模式是，与文件系统服务器类似，提供一个图形系统服务器，用于接收从其他的程序发来的请求。请求包括显示的位置，以及`canvas`。该服务器将`canvas`写入frambuffer并update。

其他程序与图形服务器通过`IPC`进行通讯。

剩余的事情就可以交给用户空间了。包括对`canvas`的处理，更新显示，添加各种元件。之前写的字库也可以不用写在内核了...

首先实现绘制`canvas`。

```c
int draw_canvas(uint16_t x, uint16_t y, canvas_t *canvas)
{
    int i, j;
    int width = (x + canvas->width) > graph.scrnx ? graph.scrnx : (x + canvas->width);
    int height = (y + canvas->height) > graph.scrny ? graph.scrny : (y + canvas->height);
    cprintf("width %d height %d\n",width,height);
    for (j = y; j < height; j++)
        for (i = x; i < width; i++)
            PIXEL(i, j) = *(canvas->data + (i - x) + (j - y) * canvas->width);
    update_screen();
    return 0;
}
```

然后在`lib`中新建`canvas`的相关方法：

```c
int canvas_init(uint16_t width, uint16_t height, canvas_t *canvas);
int canvas_draw_bg(uint8_t color, canvas_t *canvas);
int canvas_draw_ascii(uint16_t x, uint16_t y, char *str, uint8_t color, canvas_t *canvas);
int canvas_draw_cn(uint16_t x, uint16_t y, char *str, uint8_t color, canvas_t *canvas);
int canvas_draw_rect(uint16_t x, uint16_t y, uint16_t l, uint16_t w, uint8_t color, canvas_t *canvas);
```

其中只需要将原来的`PIXAL`宏换为

```c
#define CANVAS_PIXEL(canvas, x, y) *(canvas->data + x + (y * canvas->width))
```

测试`canvas`

```c
    canvas_t canvas_test;
    canvas_init(300, 200, &canvas_test);
    uint8_t testcanvas[60000];
    canvas_test.data = (uint8_t *)testcanvas;
    canvas_draw_bg(0x22,&canvas_test);
    canvas_draw_ascii((uint16_t)2, (uint16_t)2, test_ascii, (uint8_t)0xff, &canvas_test);
    canvas_draw_cn((uint16_t)2, (uint16_t)50, test_cn, (uint8_t)0xff, &canvas_test);
    draw_canvas(500, 500, &canvas_test);
```

