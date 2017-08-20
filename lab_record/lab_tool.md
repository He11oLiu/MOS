# lab tools guide

[Tools Guide](https://pdos.csail.mit.edu/6.828/2016/labguide.html#gdb)

## How to build and debug

1. 由于`make`没有自己找出`qemu`的位置，在文件`conf/env.mk`中定义`QEMU`的安装位置：

   ```shell
   QEMU=/usr/local/bin/qemu-system-i386
   ```

2. `make qemu`查看是否能够正确编译成功

3. 两个`terminal`分别运行`make qemu-gdb`与`make gdb`可以用于`debug`

   > GDB is your friend. Use the [qemu-gdb](https://pdos.csail.mit.edu/6.828/2016/labguide.html#make-qemu-gdb) target (or its [`qemu-gdb-nox`](https://pdos.csail.mit.edu/6.828/2016/labguide.html#make-qemu-gdb-nox) variant) to make QEMU wait for GDB to attach. 

4. Different ways to make

   > - make qemu
   >
   >   Build everything and start QEMU with the VGA console in a new window and the serial console in your terminal. To exit, either close the VGA window or press `Ctrl-c` or `Ctrl-a x` in your terminal.
   >
   > - make qemu-nox
   >
   >   Like `make qemu`, but run with only the serial console. To exit, press `Ctrl-a x`. This is particularly useful over SSH connections to Athena dialups because the VGA window consumes a lot of bandwidth.
   >
   > - make qemu-gdb
   >
   >   Like `make qemu`, but rather than passively accepting GDB connections at any time, this pauses at the first machine instruction and waits for a GDB connection.
   >
   > - make qemu-nox-gdb
   >
   >   A combination of the `qemu-nox` and `qemu-gdb` targets.
   >
   > - make run-*name*
   >
   >   (Lab 3+) Run user program *name*. For example, `make run-hello` runs `user/hello.c`.
   >
   > - make run-*name*-nox, run-*name*-gdb, run-*name*-gdb-nox,
   >
   >   (Lab 3+) Variants of `run-*name*` that correspond to the variants of the `qemu`target.
   >
   > The makefile also accepts a few useful variables:
   >
   > - make V=1 ...
   >
   >   Verbose mode. Print out every command being executed, including arguments.
   >
   > - make V=1 grade
   >
   >   Stop after any failed grade test and leave the QEMU output in `jos.out` for inspection.
   >
   > - make QEMUEXTRA='*args*' ...
   >
   >   Specify additional arguments to pass to QEMU.

## GDB

- Ctrl-c

  Halt the machine and break in to GDB at the current instruction. If QEMU has multiple virtual CPUs, this halts all of them.

- c (or continue)

  Continue execution until the next breakpoint or `Ctrl-c`.

- si (or stepi)

  Execute one machine instruction.

- b function or b file:line (or breakpoint)

  Set a breakpoint at the given function or line.

- b **addr* (or breakpoint)

  Set a breakpoint at the EIP *addr*.

- set print pretty

  Enable pretty-printing of arrays and structs.

- info registers

  Print the general purpose registers, `eip`, `eflags`, and the segment selectors. For a much more thorough dump of the machine register state, see QEMU's own `info registers` command.

- x/*N*x *addr*

  Display a hex dump of *N* words starting at virtual address *addr*. If *N* is omitted, it defaults to 1. *addr* can be any expression.

- x/*N*i *addr*

  Display the *N* assembly instructions starting at *addr*. Using `$eip` as *addr* will display the instructions at the current instruction pointer.

- symbol-file *file*

  (Lab 3+) Switch to symbol file *file*. When GDB attaches to QEMU, it has no notion of the process boundaries within the virtual machine, so we have to tell it which symbols to use. By default, we configure GDB to use the kernel symbol file, `obj/kern/kernel`. If the machine is running user code, say`hello.c`, you can switch to the hello symbol file using `symbol-file obj/user/hello`.

QEMU represents each virtual CPU as a thread in GDB, so you can use all of GDB's thread-related commands to view or manipulate QEMU's virtual CPUs.

- thread *n*

  GDB focuses on one thread (i.e., CPU) at a time. This command switches that focus to thread *n*, numbered from zero.

- info threads

  List all threads (i.e., CPUs), including their state (active or halted) and what function they're in.



## QEMU

QEMU includes a built-in monitor that can inspect and modify the machine state in useful ways. To enter the monitor, press Ctrl-a c in the terminal running QEMU. Press Ctrl-a c again to switch back to the serial console.

For a complete reference to the monitor commands, see the [QEMU manual](http://wiki.qemu.org/download/qemu-doc.html#pcsys_005fmonitor). Here are some particularly useful commands:

- xp/*N*x *paddr*

  Display a hex dump of *N* words starting at *physical* address *paddr*. If *N* is omitted, it defaults to 1. This is the physical memory analogue of GDB's `x`command.

- info registers

  Display a full dump of the machine's internal register state. In particular, this includes the machine's *hidden* segment state for the segment selectors and the local, global, and interrupt descriptor tables, plus the task register. This hidden state is the information the virtual CPU read from the GDT/LDT when the segment selector was loaded. Here's the CS when running in the JOS kernel in lab 1 and the meaning of each field:`CS =0008 10000000 ffffffff 10cf9a00 DPL=0 CS32 [-R-]``CS =0008`The visible part of the code selector. We're using segment 0x8. This also tells us we're referring to the global descriptor table (0x8&4=0), and our CPL (current privilege level) is 0x8&3=0.`10000000`The base of this segment. Linear address = logical address + 0x10000000.`ffffffff`The limit of this segment. Linear addresses above 0xffffffff will result in segment violation exceptions.`10cf9a00`The raw flags of this segment, which QEMU helpfully decodes for us in the next few fields.`DPL=0`The privilege level of this segment. Only code running with privilege level 0 can load this segment.`CS32`This is a 32-bit code segment. Other values include `DS` for data segments (not to be confused with the DS register), and `LDT` for local descriptor tables.`[-R-]`This segment is read-only.

- info mem

  (Lab 2+) Display mapped virtual memory and permissions. For example,`ef7c0000-ef800000 00040000 urwefbf8000-efc00000 00008000 -rw`tells us that the 0x00040000 bytes of memory from 0xef7c0000 to 0xef800000 are mapped read/write and user-accessible, while the memory from 0xefbf8000 to 0xefc00000 is mapped read/write, but only kernel-accessible.

- info pg

  (Lab 2+) Display the current page table structure. The output is similar to `info mem`, but distinguishes page directory entries and page table entries and gives the permissions for each separately. Repeated PTE's and entire page tables are folded up into a single line. For example,`VPN range     Entry         Flags        Physical page[00000-003ff]  PDE[000]     -------UWP  [00200-00233]  PTE[200-233] -------U-P 00380 0037e 0037d 0037c 0037b 0037a ..[00800-00bff]  PDE[002]     ----A--UWP  [00800-00801]  PTE[000-001] ----A--U-P 0034b 00349  [00802-00802]  PTE[002]     -------U-P 00348`This shows two page directory entries, spanning virtual addresses 0x00000000 to 0x003fffff and 0x00800000 to 0x00bfffff, respectively. Both PDE's are present, writable, and user and the second PDE is also accessed. The second of these page tables maps three pages, spanning virtual addresses 0x00800000 through 0x00802fff, of which the first two are present, user, and accessed and the third is only present and user. The first of these PTE's maps physical page 0x34b.

QEMU also takes some useful command line arguments, which can be passed into the JOS makefile using the 

QEMUEXTRA

 variable.

- make QEMUEXTRA='-d int' ...

  Log all interrupts, along with a full register dump, to `qemu.log`. You can ignore the first two log entries, "SMM: enter" and "SMM: after RMS", as these are generated before entering the boot loader. After this, log entries look like`     4: v=30 e=0000 i=1 cpl=3 IP=001b:00800e2e pc=00800e2e SP=0023:eebfdf28 EAX=00000005EAX=00000005 EBX=00001002 ECX=00200000 EDX=00000000ESI=00000805 EDI=00200000 EBP=eebfdf60 ESP=eebfdf28...`The first line describes the interrupt. The `4:` is just a log record counter. `v`gives the vector number in hex. `e` gives the error code. `i=1` indicates that this was produced by an `int` instruction (versus a hardware interrupt). The rest of the line should be self-explanatory. See [info registers](https://pdos.csail.mit.edu/6.828/2016/labguide.html#qemu-info-registers) for a description of the register dump that follows.

  Note: If you're running a pre-0.15 version of QEMU, the log will be written to `/tmp` instead of the current directory.