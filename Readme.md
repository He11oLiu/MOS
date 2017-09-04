# JOS lab

## 关于

课程网站 [MIT 6.828 website](https://pdos.csail.mit.edu/6.828/2016/)

记录文档见`./lab_record`



## 完成列表

- [x] lab1 Booting a PC
- [ ] lab1 challenge: VGA GUI
- [x] lab2 Memory Management
- [ ] lab2 challenge
- [x] lab3 User Environments
- [x] lab3 challenge: single step debug
- [ ] lab3 challenge: sysenter & sysexit
- [x] lab4 Preemptive Multitasking
- [ ] lab4 challenge
- [x] Lab 5: File system, Spawn and Shell
- [ ] lab5 challenge




## 实验中已支持的特性

- 段页式内存管理 （详见`\lab_record\lab2.md`）
- 支持进程(`Environments`)
  - 进程切换
  - 进程间通讯，通过`syscall`实现
  - 进程单独地址空间
  - `spawn`创建进程，`fork`使用`Read Copy Update`策略
  - 支持抢断式任务调度
- 支持多核`CPU`
  - 支持`IPI`，提供`IPI`接口
  - 支持大内核锁（基于自旋锁）
- 系统服务`syscall`
  - 打印字符
  - 获取字符
  - 获取进程编号
  - 回收进程
  - 主动调度
  - `fork`
  - 设置进程状态
  - 申请页，映射页，取消映射
  - **用户空间页错误处理入口设置**
  - `IPC`进程间通讯
  - 用户空间异常处理栈设置
- 支持页错误用户空间处理
- 支持简易文件系统
- 用户空间工具
  - `sh`简易`shell`



## 新特性

- 支持原子操作
- 支持读写锁
- 支持`IPI`
- 支持`PRWLock`






## 计划完成的特性

- MSH

  功能更全的`shell`

- GUI接口

  `VESA`开启`GUI`，实现简易绘图接口

- `lab challenge