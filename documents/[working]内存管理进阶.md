# 内存管理进阶 (lab2 challenge)

在`lab2`中实现的内存管理只是针对单页建立`freelist`，`list`中用链表连接起来的都是代表单页的结构体`struct PageInfo`。且每次释放页，都是丢在这个`free_list`的头。这样有几个问题：

- 不能分配大于`4k`的连续空间（后面做`frambuf`的时候要用到）
- 不断地加到空闲列表的头会使内存空间十分的混乱。不利于内存管理。

所以先要设计一种能够支持分配连续空间的机制。



## 一种简单的实现

最简单的想法就是保持现有的不动，`freelist`保证从高地址到低地址。

**这要求在`page_free`的时候做一下手脚，放到合适的位置。**

在`npages_alloc`的时候，找到连续的空闲的页即可。

#### Free

既然最主要的是在`free`的时候需要维护`freelist`按照地址的大小排列，那么就先简单将`page_free`重新写一下，找到合适的位置再进行插入操作。 

特别要注意是否刚好应该插入到`free list`的头的情况：

如果刚好是最高的地址，那么就需要修改`page_free_list`

```c
	if (page2pa(page_free_list) < page2pa(pp))
	{
		cur = page_free_list;
		page_free_list = pp;
		pp->pp_link = cur;
		return;
	}
```

否则需要遍历来查找位置插入

```c
	cur = page_free_list;
	prev = page_free_list;
	while (page2pa(cur) > page2pa(pp))
	{
		prev = cur;
		if ((cur = cur->pp_link) == NULL)
			break;
	}
	prev->pp_link = pp;
	pp->pp_link = cur;
```

写完简单的`free`之后，我们可以确保`freelist`的顺序问题了。

#### npages_alloc

再来看主要的`alloc`，其核心思想则是检查是否刚好有连续的空间能够分配出去，这里用`consecutive`来记录累计连续的页数。

通过`pageInfo`在`pages`的数组的偏移即可知道其对应的地址，如果这个偏移是连续的，则代表着一块连续的空间：

```c
(int)(cur - pages) == (int)(prev - pages) - 1
```

其中`cur`为当前遍历到的`pageInfo`，而`prev`是上次遍历的，通过上面的表达式可以判断是否为连续。

如果找到了合适的一块空间，则需要

- 维护`freelist`，将这块空间前的最后一页连接到分配走的后面一页。

  同样注意是否有存在需要换头的情况

  ```c
  		if (pp == page_free_list)
  			page_free_list = cur;
  		else
  			pp_prev->pp_link = cur;
  ```

- 初始化页属性与空间

  ```c
  		if (alloc_flags & ALLOC_ZERO)
  			memset(page2kva(prev), 0, n * PGSIZE);
  		// clear pp link
  		for (i = 0; i < n; i++)
  			(prev + i)->pp_link = NULL;
  		return prev;
  ```

  完整的`npages_alloc`见下：

```c
struct PageInfo *npages_alloc(unsigned int n, int alloc_flags)
{
	struct PageInfo *cur;
	struct PageInfo *prev;
	struct PageInfo *pp;
	struct PageInfo *pp_prev;
	unsigned int i;
	unsigned int consecutive = 1;
	if (page_free_list == NULL)
		return NULL;
	pp = page_free_list;
	pp_prev = page_free_list;
	prev = page_free_list;
	cur = page_free_list->pp_link;

	while (consecutive < n && cur != NULL)
	{
		if ((int)(cur - pages) != (int)(prev - pages) - 1)
		{
			consecutive = 1;
			pp_prev = prev;
			pp = cur;
		}
		else
			consecutive++;
		prev = cur;
		cur = cur->pp_link;
	}
	if (consecutive == n)
	{
		// alloc flags
		if (alloc_flags & ALLOC_ZERO)
			memset(page2kva(prev), 0, n * PGSIZE);
		// update page_free_list
		if (pp == page_free_list)
			page_free_list = cur;
		else
			pp_prev->pp_link = cur;
		// clear pp link
		for (i = 0; i < n; i++)
			(prev + i)->pp_link = NULL;
		return prev;
	}
	return NULL;
}
```

#### kmalloc

实现了`npages_alloc`，再来实现`malloc`就简单了，主要两个问题

- 需要分配多少页？ 通过`ROUNDUP`后再除以页大小即可。
- 其对应的虚拟地址是多少？ 利用`page2kva`转换。

完整的`kmalloc`如下。

```c
void *kmalloc(size_t size)
{
	struct PageInfo *pp;
	int npages;
	size = ROUNDUP(size, PGSIZE);
	npages = size / PGSIZE;
	if ((pp = npages_alloc(npages, 1)) == NULL)
		return NULL;
	return page2kva(pp);
}
```

此时已经可以测试是否基本正确。

#### npages_free

为了实现`free`，还需要实现`npages_free`，这个和之前实现的思路相同。

主要注意如何连接起`freelist`。

```c
	prev->pp_link = pp + n - 1;
	pp->pp_link = cur;
	for (i = 1; i < n; i++)
		(pp + i)->pp_link = pp + i - 1;
```

其中`prev`是合适位置的之前一个，`cur`是合适位置的下一个。

`npages_free`完整实现见下。

```c
void npages_free(struct PageInfo *pp, unsigned int n)
{
	struct PageInfo *cur, *prev;
	unsigned int i;
	for (i = 0; i < n; i++)
	{
		if ((pp + i)->pp_ref)
			panic("npages_free error: (pp+%d)->pp_ref != 0", i);
		if ((pp + i)->pp_link != NULL)
			panic("npages_free error: (pp+%d)->pp_link != NULL", i);
	}
	if (page2pa(page_free_list) < page2pa(pp))
	{
		cur = page_free_list;
		page_free_list = pp + n - 1;
		pp->pp_link = cur;
		for (i = 1; i < n; i++)
			(pp + i)->pp_link = pp + i - 1;
		return;
	}
	cur = page_free_list;
	prev = page_free_list;
	while (page2pa(cur) > page2pa(pp))
	{
		prev = cur;
		if ((cur = cur->pp_link) == NULL)
			break;
	}
  	// test use
	cprintf("find prev %d cur %d\n", (int)(prev - pages), (int)(cur - pages));

	prev->pp_link = pp + n - 1;
	pp->pp_link = cur;
	for (i = 1; i < n; i++)
		(pp + i)->pp_link = pp + i - 1;
	return;
}
```

#### `kfree`

和`kmalloc`类似，算出大小释放即可

```c
void kfree(void *kva, size_t size)
{
	struct PageInfo *pp = pa2page(PADDR(kva));
	int npages;
	size = ROUNDUP(size, PGSIZE);
	npages = size / PGSIZE;
	npages_free(pp, npages);
}
```

#### 兼容单页分配

为了和已经写的兼容，直接调用`npages_xx`即可

```c
struct PageInfo *page_alloc(int alloc_flags)
{
	return npages_alloc(1, alloc_flags);
}
void page_free(struct PageInfo *pp)
{
	npages_free(pp, 1);
}
```

#### 测试

首先取消有关内存的几个`check`，有几个原因导致：

- `free`后不是放在头的
- `check page`中做了`mmio`映射，从而`kern pgdir`中对应的`pd`就是`PTE_P`

所以`check`除了第一个全部取消。

#### freelist free后顺序

首先是单页`free`后的顺序测试

```c
	struct PageInfo *alloc1 = page_alloc(1);
	struct PageInfo *alloc2 = page_alloc(1);
	struct PageInfo *alloc3 = page_alloc(1);
	cprintf("alloc 1 at %d\n", (int)(alloc1 - pages));
	cprintf("alloc 2 at %d\n", (int)(alloc2 - pages));
	cprintf("alloc 3 at %d\n", (int)(alloc3 - pages));
	page_free(alloc1);
	page_free(alloc3);
```

刚才`free`中写了测试语句，输出如下：

```c
alloc 1 at 1023
alloc 2 at 1022
alloc 3 at 1021
find prev 1023 cur 1020
```

#### malloc/free 测试

之前在写显存的双缓冲的时候，委曲求全选择了静态分配。这里重新使用动态分配并进行测试：

```c
void init_framebuffer(){
    void *malloc_free_test;
    if((framebuffer = (uint8_t *) kmalloc((size_t)(graph.scrnx*graph.scrny)))== NULL)
        panic("kmalloc error!");
    malloc_free_test = framebuffer;
    kfree(framebuffer,(size_t)(graph.scrnx*graph.scrny));    
    if((framebuffer = (uint8_t *) kmalloc((size_t)(graph.scrnx*graph.scrny)))== NULL)
        panic("kmalloc error!");
    if(malloc_free_test == framebuffer)
        cprintf("kmalloc/kfree check success\n");
    else
        panic("kmalloc/kfree error!\n");

    // framebuffer = tmpbuf;
    if(framebuffer == NULL)
        panic("Not enough memory for framebuffer!");
}
```

测试输出正确。



## 更高效的空闲链表设计

上面说过，原来的空闲链表是连接的一个一个页的信息。但是由于`JOS`在设计的时候希望能够每个页有一个对应的页信息。并利用此来从`pageinfo`找到`kva`。所以设计的新的`pageinfo`结构体仍然是每个页拥有一个。

#### Free Area

新增一个概念：`Free Area`。所谓`Free Area`则是空闲页连接起来的一片区域，直到下一个被使用的页。

这将涉及到两个重要的信息：

- `FreeArea`的第一个页是谁
- `FreeArea`的大小是多少

以及分配，释放的策略的不同。这里使用`First fit`的策略来分配页。

#### 新的`PageInfo`结构体

设计的新的`PageInfo`结构体：

```c
#define FIRSTPAGE 0x1
struct PageInfo {
	// Old:Next page on the free list.
  	// New:Fist page in next free area.
	struct PageInfo *pp_link;
  	// some infomation about this page
	uint8_t flags;
  	// size of this free area.
  	uint32_t  freesize;
	// pp_ref is the count of pointers (usually in page table entries)
	// to this page, for pages allocated using page_alloc.
	// Pages allocated at boot time using pmap.c's
	// boot_alloc do not have valid reference count fields.
	uint16_t pp_ref;
};
```

#### 具体分配策略与释放策略

##### npages_alloc

分配一个大小为`n pages`的页，要遍历`free list`。

这里的`free list`保存的不再是一页页的链表，而是`Free area`的链表。

找到第一个`freesize > n`的区域，分配出去，并且设置后面的一页为新的`Area`头。

#### npages_free

释放一个大小为`n pages`的页，也要遍历`free list`，找到地址在其前的进的看能不能加入它的`area`，找到地址在其后的，看看能否加入。不能的话需要插入一个新的独立的`area`。



类似的思路在`ucore`中写过了，在`JOS`中由于`init`的时候比较复杂，链表的链接问题比较严重，这里就不尝试了。关于`ucore`的实现可见我的[CSDN 博客](http://blog.csdn.net/he11o_liu/article/details/54028501)。