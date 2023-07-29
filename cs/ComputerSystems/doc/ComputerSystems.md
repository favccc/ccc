# Computer Systems - A programmer's Perspective - Third Edition

本书作者：Randal E.Bryant, David R.O'Hallaron

本书译者：龚奕利 贺莲

出版社：机械工业出版社

版权声明：本文大量摘录了原书内容，仅供个人学习之用，请勿用作其它用途；

## 第1章 计算机系统漫游

计算机系统是由硬件和系统软件组成的，它们共同工作来运行程序。虽然系统的具体实现方式随着时间不断变化，但是系统内在的概念却没有变化。

```c
#include <stdio.h>

int main()
{
    printf("hello, world\n");
    return 0;
}
```

以上程序为hello程序。我们通过追踪hello程序的声明周期来开始对系统的学习：从它被程序员创建开始，到在系统上运行，输出简单的信息，然后终止。我们将沿着这个程序的生命周期，简要地介绍一些逐步出现的关键概念、专业术语和组成部分。后面的章节将围绕这些内容展开。

### 1.1 信息就是位+上下文

hello程序的生命周期是从一个源程序开始的。源程序实际上就是一个由值0和1组成的位序列，8个位被组织成一组，称为字节。每个字节表示程序中的某些文本字符。

大部分的现代计算机系统都使用ASCII标准来表示文本字符，这种方式实际上就是用一个唯一的单字节大小的整数值来表示每个字符。

hello.c的表示方法说明了一个基本思想：系统中的所有信息，包括磁盘文件、内存中的程序、内存中存放的用户数据以及网络上传送的数据，都是由一串比特表示的。区分不同数据对象的唯一方法是我们谈到这些数据对象时的上下文。

### 1.2 程序被其它程序翻译成不同的格式

hello程序的声明周期是从一个高级C语言程序开始的，因为这种形式能够被人读懂。然而，为了在系统上运行hello.c文件，每条C语句都必须被其它程序转换为一系列的低级机器语言指令。然后这些指令按照一种称为可执行目标程序的格式打好包，并以二进制磁盘文件的形式被存放起来。目标程序也称为可执行目标文件。

![](assets/1_2_1.svg)

### 1.3 了解编译系统如何工作是大有益处的

对于像hello.c这样简单的程序，我们可以依靠变异系统生成正确有效的机器代码。但是，有一些重要的原因促使程序员必须知道编译系统是如何工作的。

- 优化程序性能。现代编译器都是成熟的工具，通常可以生成很好的代码。作为程序员，我们无须为了写出高效的代码而去了解编译器的内部工作。但是，为了在C程序中做出好的编码选择，我们确实需要了解一些机器代码以及编译器将不同的C语句转化为机器代码的方式。

- 理解衔接时出现的错误。根据我们的经验，一些最令人困扰的程序错误往往都与链接器操作有关，尤其是当你构建大型的软件系统时。

- 避免安全漏洞。多年来，缓冲区溢出错误是造成大多数网络和Internet服务器上安全漏洞的主要原因。存在这些错误是因为很少有程序员能够理解需要限制从不受信任的源接收数据的数量和格式。学习安全编程的第一步就是理解数据和控制信息存储在程序栈上的方式会引起的后果。

### 1.4 处理器读并解释储存在内存中的指令

此刻，hello.c源程序已经被编译系统翻译成了可执行目标文件hello，并被存放在磁盘上。要想在Unix系统上运行该可执行文件，我们将它的文件名输入到称为shell的应用程序中。

shell是一个命令行解释器，它输出一个提示符，等待输入一个命令，然后执行这个命令。

#### 1.4.1 系统的硬件组成

一个典型系统的硬件组织，如下图所示：

![](assets/1_4_1_1.svg)

1. 总线

   贯穿整个系统的是一组电子管道，称作总线，它携带信息字节并负责在各个部件间传递。通常总线被设计成传送定长的字节块，也就是字（word）。字中的字节数是一个基本的系统参数，各个系统中都不尽相同。现在大多数机器字长哟啊么是4个字节，要么是8个字节。

2. IO设备

   IO设备是系统与外部世界的联系通道。每个IO设备都通过一个控制器或适配器与IO总线相连。控制器与适配器之间的主要区别在于它们之间的封装方式。控制器是IO设备本身或者系统的主印制电路板上的芯片组。而适配器则是一块插在主板插槽上的卡。无论如何，它们的功能都是在IO总线和IO设备之间传递信息。

3. 主存

   主存是一个临时存储设备，在处理器执行程序时，用来存放程序和程序处理的数据。从物理上来说，主存是由一组动态随机存储器（DRAM）芯片组成的。从逻辑上来说，存储器是一个线性的字节数组，每个字节都有其唯一的地址（数组索引），这些地址是从零开始的。

4. 处理器

   中央处理单元（CPU），简称处理器，是解释（或执行）存储在主存中指令的引擎。处理器的核心是一个大小为一个字的存储设备（或寄存器），称为程序计数器（PC）。在任何时刻，PC都指向主存中的某条机器语言指令（即含有该条指令的地址）。

   从系统通电开始，直到系统断电，处理器一直在不断地执行程序计数器指向的指令，再更新程序计数器，使其指向下一条指令。

   寄存器文件是一个小的存储设备，有一些单个字长的寄存器组成，每个寄存器都有唯一的名字。ALU计算新的数据和地址值。

处理器看上去是它的指令集架构的简单实现，但是实际上现代处理器使用了非常复杂的机制来加速程序的执行。因此，我们将处理器的指令集架构和处理器的为体系结构区分开来：指令集架构描述的是每条机器代码指令的效果，而微体系结构描述的是处理器实际上是如何实现的。

#### 1.4.2 运行hello程序

初始时，shell程序执行它的指令，等待我们输入一个命令。当我们在键盘上输入字符串"./hello"之后，shell程序将字符逐一读入寄存器，再把它存放到内存中。

当我们在键盘上敲回车键时，shell程序就知道我们已经结束了命令的输入。然后shell执行一系列指令来加载可执行的hello文件，这些指令将hello目标文件中的代码和数据从磁盘复制到主存。数据包括最终会被输出的字符串“hello, world\n”。

利用直接存储器存取（DMA）技术，数据可以不通过处理器而直接从磁盘到达主存。

一旦目标文件hello中的代码和数据被加载到主存，处理器就开始执行hello程序的main程序中的机器语言指令。这些指令将“hello, world\n”字符串中的字节从主存复制到寄存器文件中，再从寄存器文件中复制到显示设备，最终显示在屏幕上。

### 1.5 高速缓存至关重要

这个简单的示例揭示了一个重要的问题，即系统话费了大量的时间把信息从一个地方挪到另一个地方。因此系统设计者的一个主要目标就是使这些复制操作尽可能快地完成。

根据机械原理，较大的存储设备要比较小的存储设备运行得慢，而快速设备的造价远高于同类的低速设备。比如，一个典型系统上的磁盘驱动器可能比主存大1000倍，但是对处理器而言，从磁盘驱动器上读取一个字的时间开销要比从主存中读取的开销大1000万倍。类似的，一个典型的寄存器文件只存储几百字节的信息，而主存里可存放几十亿字节。然而，处理器从寄存器文件中读数据比从主存中读取几乎要快100倍。更麻烦的是，随着半导体技术的进步，这种处理器与主存之间的差距还在持续增大。

针对这种处理器与主存之间的差异，系统设计者采用了更小更快的存储设备，称为告诉缓存存储器（cache memory），作为暂时的集结区域，存放处理器近期可能会需要的信息。

![](assets/1_5_1.svg)

位于处理器芯片上的L1高速缓存的容量可以达到数万字节，访问速度几乎和访问寄存器文件一样快。一个容量为数十万到数百万字节的更大的L2缓存通过一条特殊的总线连接到处理器。L1和L2 高速缓存是用一种叫做静态随机访问存储器（SRAM）的硬件技术实现的。比较新的、处理能力更强大的系统甚至有三级高速缓存。

通过让高速缓存里存放可能经常访问的数据，大部分的内存操作都能在快速的高速缓存中完成。

本书得出的重要结论之一就是，意识到高速缓存存储器存在的应用程序员能够利用高速缓存将程序的性能提高一个数量级。

### 1.6 存储设备形成层次结构

在处理器和一个较大较慢的设备之间插入一个更小更快的存储设备的想法已经成为一个普遍的观念。实际上，每个计算机系统中的存储设备都被组织成了一个存储器层次结构。在这个层次结构中，设备的访问速度越来越慢，容量越来越大，并且每字节的造价也越来越便宜。

### 1.7 操作系统管理硬件
