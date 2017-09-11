# 在macOS上搭建JOS编译运行环境

## Tools we need

在搭建环境之前，首先macOS上需要有以下两个工具：

- `Homebrew` [*Homebrew — The missing package manager for macOS*](https://brew.sh/)
- `MacPorts` [The *MacPorts* Project -- Home](http://www.baidu.com/link?url=81LcppTMUWAiK3PYa9pVTKx0TW2NrOFMGisnjCJ_IechHtiRzN0kbC0ydOtP5C7q)



## 运行`JOS`

- `QEMU`

  有了`Homebrew`，直接利用`brew`安装即可安装（自动安装依赖库）

  ```
  $brew install qemu
  ```

- 将`kernel.img`与`fs.img`放在目标目录下 （也可以在其他位置，为了下面的`Makefile`好写）

  ```
  .
  ├── Makefile
  ├── fs.img
  └── kernel.img
  ```

- 书写`Makefile`

  ```makefile
  QEMU=/usr/local/Cellar/qemu/2.10.0/bin/qemu-system-i386 # path to qemu
  run:
  	$(QEMU) -drive file=./kernel.img,index=0,media=disk,format=raw -serial mon:stdio -vga std -smp 1 -drive file=./fs.img,index=1,media=disk,format=raw
  ```

## 编译`JOS`

- `i386-elf-gcc`

  利用`Macports`来安装`i386-elf-gcc`

  ```
  $ sudo port -v selfupdate
  $ sudo port install i386-elf-gcc
  ```

  `Macports`会帮你下载源码，编译（非常漫长）

- 修改`Makefile`中的一些内容

  ```shell
  diff --git a/GNUmakefile b/GNUmakefile
  index adc693e..60fe010 100644
  --- a/GNUmakefile
  +++ b/GNUmakefile
  @@ -33,15 +33,15 @@ TOP = .

   # try to infer the correct GCCPREFIX
   ifndef GCCPREFIX
  -GCCPREFIX := $(shell if i386-jos-elf-objdump -i 2>&1 | grep '^elf32-i386$$' >/dev/null 2>&1; \
  -       then echo 'i386-jos-elf-'; \
  +GCCPREFIX := $(shell if i386-elf-objdump -i 2>&1 | grep '^elf32-i386$$' >/dev/null 2>&1; \
  +       then echo 'i386-elf-'; \
          elif objdump -i 2>&1 | grep 'elf32-i386' >/dev/null 2>&1; \
          then echo ''; \
          else echo "***" 1>&2; \
          echo "*** Error: Couldn't find an i386-*-elf version of GCC/binutils." 1>&2; \
  -       echo "*** Is the directory with i386-jos-elf-gcc in your PATH?" 1>&2; \
  +       echo "*** Is the directory with i386-elf-gcc in your PATH?" 1>&2; \
          echo "*** If your i386-*-elf toolchain is installed with a command" 1>&2; \
  -       echo "*** prefix other than 'i386-jos-elf-', set your GCCPREFIX" 1>&2; \
  +       echo "*** prefix other than 'i386-elf-', set your GCCPREFIX" 1>&2; \
          echo "*** environment variable to that prefix and run 'make' again." 1>&2; \
          echo "*** To turn off this error, run 'gmake GCCPREFIX= ...'." 1>&2; \
          echo "***" 1>&2; exit 1; fi)
  ```

- 修改`.deps`中一些内容

  删除`fsformat`的依赖检查

  ```shell
  obj/fs/: fs/fsformat.c
  ```

- 修改配置文件中的`qemu`参数

  ```
  QEMU=/usr/local/Cellar/qemu/2.10.0/bin/qemu-system-i386
  ```