# GUI

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



## 图像处理的两种设计与遇到的问题

- 第一种设计与之前描述的一致：

  提供一个图像服务器，接收请求，从用户进程传来需要画的画布和显示位置，并在位置上进行绘画。

  这种方式遇到的问题是画布过大，一页可能装不下。需要`mmap`（还没写）

- 第二种设计是一个`launcher`和`application`两个单独的单页面切换制度。

  这样就是`launcher`提供应用启动界面，`application`提供应用界面。

  但是，这就需要在内存中开辟一片内核和用户均能看到的位置，然后一方写，一方读。这会破坏现有的`memory layout`，实现不够优雅。

重新回顾了一下内存分配，内核与用户态数据共享的方法后，决定先就第二个思路实现一个简单的用户内核均可见可读写的`Framebuffer`。

#### 分析如何做才能内核用户均可读写

**首先分析一个之前做过的`pages`，是如何做到用户态可以读，内核态可以写的。**

- 在`mem_init`的时候在在内核空间中分配指定的空间给`pages`

  ```c
  pages = boot_alloc(sizeof(struct PageInfo) * npages);
  memset(pages, 0, sizeof(struct PageInfo) * npages);
  ```

- 利用`boot_map_region`将其映射到内核页表中的`UPAGES`的位置。

  ```c
  boot_map_region(kern_pgdir, UPAGES, PTSIZE, PADDR(pages), PTE_U | PTE_P);
  ```

- 这样内核中依然可以通过`pages`访问页表，而用户程序在`entry`的时候通过给`pages`变量赋予存储位置

  ```
  	.globl pages
  	.set pages, UPAGES
  ```

  也可以通过`pages`变量进行访问。

#### 预留内存用于`framebuffer`

再思考如果需要这么一个`framebuffer`，我们需要放到哪里。仿造上面的`UVPD`，`UPAGES`，等，决定就放在接近`ULIM`的位置。一个`PTSIZE`也远超我们需要的空间，为以后扩展也留下了余量。

```c
/*
 * ULIM, MMIOBASE -->  +------------------------------+ 0xef800000
 *                     |  Cur. Page Table (User R-)   | R-/R-  PTSIZE
 *    UVPT      ---->  +------------------------------+ 0xef400000
 *                     |          RO PAGES            | R-/R-  PTSIZE
 *  FRAMEBUF    ---->  +------------------------------+ 0xef000000
 *                     |        FRAME BUFFER          | RW/RW  PTSIZE
 *    UPAGES    ---->  +------------------------------+ 0xeec00000
 *                     |           RO ENVS            | R-/R-  PTSIZE
 * UTOP,UENVS ------>  +------------------------------+ 0xee800000
 */

// User read-only virtual page table (see 'uvpt' below)
#define UVPT (ULIM - PTSIZE)
// Read-only copies of the Page structures
#define UPAGES (UVPT - PTSIZE)
// Read-write framebuffer
#define FRAMEBUF (UPAGES - PTSIZE)
// Read-only copies of the global env structures
#define UENVS (FRAMEBUF - PTSIZE)
// #define UENVS (UPAGES - PTSIZE)
```

#### 什么时候映射到内核的页表？

由于图像初始化在内存初始化之后，需要留一个接口来进行映射。（`boot_map`是隐式函数）

```c
void map_framebuffer(void *kva)
{
	boot_map_region(kern_pgdir, FRAMEBUF, PTSIZE, PADDR(kva), PTE_W | PTE_U | PTE_P);
}
```

在分配好内核中的`Framebuffer`就可以开始映射了

```c
void init_framebuffer()
{
    if ((framebuffer = (uint8_t *)kmalloc((size_t)(graph.scrnx * graph.scrny))) == NULL)
        panic("Not enough memory for framebuffer!");
    map_framebuffer(framebuffer);
}
```

#### 用户程序如何访问？

在`libmain`的时候初始化即可

```c
	framebuffer = (uint8_t *)FRAMEBUF;
```

#### 用户态刷新屏幕？

用户程序在写完`frambuffer`后，如何才能刷新屏幕？这又需要一个新的内核调用

```c
static int sys_updatescreen()
{
	update_screen();
	return 0;
}
```

配套的一些代码就不解释了。



## 用户GUI接口

因为时间不多，打算简单做一个可用的`GUI`。这个`GUI`没有鼠标，全部依靠键盘控制，与功能机的操作逻辑类似。



## Bitmap图片显示

以下内容[参考文献](http://www.brackeen.com/vga/bitmaps.html)

> There are many file formats for storing bitmaps, such as RLE, JPEG, TIFF, TGA, PCX, BMP, PNG, PCD and GIF. The bitmaps studied in this section will be 256-color bitmaps, where eight bits represents one pixel.
>
> One of the easiest 256-color bitmap file format is Windows' BMP. This file format can be stored uncompressed, so reading BMP files is fairly simple.

`Windows' BMP`是没有压缩过的，所以读这种`BMP`会非常方便。这里也准备就支持这种格式的图片。

> There are a few different sub-types of the BMP file format. The one studied here is Windows' RGB-encoded BMP format. For 256-color bitmaps, it has a 54-byte header (Table III) followed by a 1024-byte palette table. After that is the actual bitmap, which starts at the lower-left hand corner.

BMP的文件格式如下：

| Data                     | Description                        |
| ------------------------ | ---------------------------------- |
| `WORD Type;`             | File type. Set to "BM".            |
| `DWORD Size;`            | Size in BYTES of the file.         |
| `DWORD Reserved;`        | Reserved. Set to zero.             |
| `DWORD Offset;`          | Offset to the data.                |
| `DWORD headerSize;`      | Size of rest of header. Set to 40. |
| `DWORD Width;`           | Width of bitmap in pixels.         |
| `DWORD Height;`          | Height of bitmap in pixels.        |
| `WORD Planes;`           | Number of Planes. Set to 1.        |
| `WORD BitsPerPixel;`     | Number of bits per pixel.          |
| `DWORD Compression;`     | Compression. Usually set to 0.     |
| `DWORD SizeImage;`       | Size in bytes of the bitmap.       |
| `DWORD XPixelsPerMeter;` | Horizontal pixels per meter.       |
| `DWORD YPixelsPerMeter;` | Vertical pixels per meter.         |
| `DWORD ColorsUsed;`      | Number of colors used.             |
| `DWORD ColorsImportant;` | Number of "important" colors.      |

## 巨坑1



其中比较坑的是，需要为2字节的倍数

```c
#pragma pack(2)
```



## 巨坑2

