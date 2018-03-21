<h1><center>Jos Lab 1: Booting a PC</center></h1>


## Part 1: PC Bootstrap

``` javascript
0xffff0:    ljmp    $0xf000,$0xe05b
```
首先开机启动执行第一条指令(`0xffff0`)处，设置`cs`寄存器的值为`0xffff0`，然后跳转到`16 * 0xf000 + 0xe05b = 0xfe05b`地址处。
``` javascript
0xfe05b:    cmpl    $0x0,%cs:0x70b8
0xfe062:	jne     0xfd408
0xfe066:	xor     %dx,%dx
0xfe068:	mov     %dx,%ss
0xfe06a:	mov     $0x7000,%esp
0xfe070:	mov     $0xf2db1,%edx
0xfe076:	jmp     0xfd28b
```
设置部分寄存器的值，将栈帧调整到`0x7000`处，跳转到`0xfd28b`处。
``` javascript
0xfd28b:    cli
```
利用`cli`关闭中断   
下面是对相应的硬件进行操作，相关端口信息可参照：http://bochs.sourceforge.net/techspec/PORTS.LST
``` javascript
0xfd28c:    cld
0xfd28d:    mov     %eax,%ecx
0xfd290:    mov     $0x8f,%eax
0xfd296:    out     %al,$0x70   
0xfd298:    in      $0x71,%al
```
`cld`标识清方向标志(`DF=0`)，然后对`CMOS`进行操作。  
`CMOS`:存储计算机启动是必须的数据，例如时钟控制、磁盘驱动、键盘、声卡、PCI总线等信息，还可以控制响应不可屏蔽中断`NMI(Non-Maskable Interrupt)`。首先将不可屏蔽中断关闭掉，然后选择`0xf`号寄存器作为读取对象，利用`in $0x71,%al`读到`%al`寄存器上
`0xF`号寄存器为`Shutdown Status Byte`(http://web.archive.org/web/20040603005903/http://members.iweb.net.au:80/%7Epstorr/pcbook/book5/cmoslist.htm)。根据`any write to 0070 should be followed by an action to 0071 or the RTC wil be left in an unknown state.`需要在`out`语句后面加上`in $0x71,%al`(https://stackoverflow.com/questions/42593957/bios-reads-twice-from-different-port-to-the-same-register-in-a-row)
``` javascript
0xfd29a:    in      $0x92,%al
0xfd29c:	or      $0x2,%al
0xfd29e:	out     %al,$0x92
```
上面这条语句是对`PS/2 system control port A`进行操作，将第一位设置成1，而根据`bit 1 = 1 indicates A20 active`，则这条语句的作用是打开`A20`地址线。
``` javascript
0xfd2a0:	mov    %ecx,%eax
0xfd2a3:	lidtw  %cs:0x70a8
0xfd2a9:	lgdtw  %cs:0x7064
```
分别将`IDT`表(地址：`0xf70a8`)和`GDT`表(地址：`0xf7064`)加载到`IDTF`和`GDTR`当中。
``` javascript
0xfd2af:	mov    %cr0,%ecx
0xfd2b2:	and    $0x1fffffff,%ecx
0xfd2b9:	or     $0x1,%ecx
0xfd2bd:	mov    %ecx,%cr0
```
上面的步骤将`cr0`寄存器的第1位(`PE`)置1，进入`Protected Mode`状态。
``` javascript
0xfd2c0:	ljmpl  $0x8,$0xfd2c8
The target architecture is assumed to be i386
0xfd2c8:	mov    $0x10,%ecx
0xfd2cd:	mov    %ecx,%ds
0xfd2cf:	mov    %ecx,%es
0xfd2d1:	mov    %ecx,%ss
0xfd2d3:	mov    %ecx,%fs
0xfd2d5:	mov    %ecx,%gs
```
上面语句为跳转到`0xfd2c8`处继续执行，并将`cs`寄存器置`0x8`，其他的段寄存器置`0x10`
``` javascript
0xfd2d7:	jmp    *%edx
0xf2db1:	push   %ebx
0xf2db2:	sub    $0x2c,%esp
```
之后的代码是对`BIOS`进行一系列的操作，初始化设备（VGA），从硬盘上面读取加载的代码(`boot loader`)，然后将控制转移到`boot loader`。

## Part 2: PC Bootstrap
磁盘最小的单位是`sector`(512bytes)。当`BIOS`发现可以启动的盘的时候，则将它的第一个扇区加载到`0x7c00`处，然后用跳转指令设置`CS:IP`为 `0000:7c00`，将控制交给`boot loader`。   
利用`CR-ROM`启动比传统的启动方式出现得更晚一些，`CD-ROM`的启动读取单位为2048bytes，比传统的启动方式复杂一些，但是功能也更加强大，这里不做详细介绍。    
`boot loader`做了两件事情：
- 将`real mode`转化成为32位下的`protected mode`，只有在这种`protected mode`下CPU才访问位1MB之上的内存空间。
- `boot loader`直接从磁盘当中读取内核的代码，加载到内存当中，并将控制权转交给内核。

#### Exercise3
> At what point does the processor start executing 32-bit code? What exactly causes the switch from 16- to 32-bit mode?  
   
http://www.blogfshare.com/gdt-ldt.html  
在语句`ljmp $PROT_MODE_CSEG, $protcseg`之后进入32位的寻址模式。
寻址模式的转变有两个触发条件：  
- `CR0`寄存器当中的`PE`位由0变成1   
- 段选择器`%cs`被设置成`$PROT_MODE_CSEG`(0x8)，表明从全局表(`GDT`)当中的第0个条目得到地址翻译信息。   
- 同时只有在CPU真正访问内存地址的过程中，才会用到转变后的寻址模式。

> What is the last instruction of the boot loader executed, and what is the first instruction of the kernel it just loaded?     

最后一句执行的语句是：  
    ```
     ((void (*)(void)) (ELFHDR->e_entry))();
    ```   
    kernel被加载之后第一条执行的语句是：   
    ``` javascript
    0x10000c:	movw   $0x1234,0x472
    ```
> How does the boot loader decide how many sectors it must read in order to fetch the entire kernel from disk? Where does it find this information?
    
``` javascript
// load each program segment (ignores ph flags)
ph = (struct Proghdr *) ((uint8_t *) ELFHDR + ELFHDR->e_phoff);
eph = ph + ELFHDR->e_phnum;
for (; ph < eph; ph++)
    // p_pa is the load address of this segment (as well
    // as the physical address)
    readseg(ph->p_pa, ph->p_memsz, ph->p_offset);
```
此语句中，`eph = ph + ELFHDR->e_phnum`表示应该从磁盘上读取多少个数据来加载内核

`ELF`是有关程序加载的一些信息，在`ELF`后面是程序的几个连续的部分，包括程序的指令(`.text`段)，程序的一些数据(`.rodata`段 `.data`段)等。

#### Exercise 5 
> Reset the machine (exit QEMU/GDB and start them again). Examine the 8 words of memory at 0x00100000 at the point the BIOS enters the boot loader, and then again at the point the boot loader enters the kernel. Why are they different? What is there at the second breakpoint? (You do not really need to use QEMU to answer this question. Just think.)
    
- the point the BIOS enters：地址0x00100000的数据为全0
- the point the boot loader enters the kernel：  
`0x1badb002	0x00000000	0xe4524ffe	0x7205c766`
我们可以看到，`kernel`的`ELF`信息为：
```
    Idx Name          Size      VMA       LMA       File off  Algn
      0 .text         00001a91  f0100000  00100000  00001000  2**4
                      CONTENTS, ALLOC, LOAD, READONLY, CODE
      1 .rodata       00000780  f0101aa0  00101aa0  00002aa0  2**5
                      CONTENTS, ALLOC, LOAD, READONLY, DATA
      2 .stab         000039b5  f0102220  00102220  00003220  2**2
                      CONTENTS, ALLOC, LOAD, READONLY, DATA
      3 .stabstr      0000197b  f0105bd5  00105bd5  00006bd5  2**0
                      CONTENTS, ALLOC, LOAD, READONLY, DATA
      4 .data         0000a300  f0108000  00108000  00009000  2**12
                      CONTENTS, ALLOC, LOAD, DATA
      5 .bss          00000660  f0112300  00112300  00013300  2**5
                      ALLOC
      6 .comment      00000034  00000000  00000000  00013300  2**0
                      CONTENTS, READONLY
```
而`boot`的为：
```
    Idx Name          Size      VMA       LMA       File off  Algn
      0 .text         00000186  00007c00  00007c00  00000074  2**2
                      CONTENTS, ALLOC, LOAD, CODE
      1 .eh_frame     000000a8  00007d88  00007d88  000001fc  2**2
                      CONTENTS, ALLOC, LOAD, READONLY, DATA
      2 .stab         00000714  00000000  00000000  000002a4  2**2
                      CONTENTS, READONLY, DEBUGGING
      3 .stabstr      00000860  00000000  00000000  000009b8  2**0
                      CONTENTS, READONLY, DEBUGGING
      4 .comment      00000034  00000000  00000000  00001218  2**0
                      CONTENTS, READONLY
```
可得知`boot`程序会从`0x7c00`出加载代码，而`kernel`程序会从`0x10000`处加载代码，所以当`kernel`程序开始运行时，`0x100000`处会加载相应的代码。
#### Exercise 6
> Trace through the first few instructions of the boot loader again and identify the first instruction that would "break" or otherwise do the wrong thing if you were to get the boot loader's link address wrong. Then change the link address in boot/Makefrag to something wrong, run make clean, recompile the lab with make, and trace into the boot loader again to see what happens. Don't forget to change the link address back and make clean afterwards!  
    
将`~/boot/Markfrag`中   
`$(V)$(LD) $(LDFLAGS) -N -e start -Ttext 0x7c00 -o $@.out $^`改成  
`$(V)$(LD) $(LDFLAGS) -N -e start -Ttext 0x7b00 -o $@.out $^`  
然后利用gdb单步调试，发现语句由  
``` javascript
[   0:7c2d] => 0x7c2d:	ljmp   $0x8,$0x7c32
The target architecture is assumed to be i386
0x7c32:	mov    $0x10,%ax
0x7c36:	mov    %eax,%ds
```
变成
``` javascript
[   0:7c2d] => 0x7c2d:	ljmp   $0x8,$0x7b32
[f000:e05b]    0xfe05b:	cmpl   $0x0,%cs:0x70b8
[f000:e062]    0xfe062:	jne    0xfd408
[f000:d408]    0xfd408:	cli
```

`boot loader`的功能是设置一些寄存器的值`DS` `ES` `SS`, 加载`GDT`表，然后切换成32-bit`protected mode`的寻址模式，并将内核的代码加载到内存当中，然后将控制转移到内核的代码上。

## Part 3: The Kernel

#### Exercise 7
https://buweilv.github.io/2017/06/10/jos-bootstrap-3/
> Use QEMU and GDB to trace into the JOS kernel and find where the new virtual-to-physical mapping takes effect. Then examine the Global Descriptor Table (GDT) that the code uses to achieve this effect, and make sure you understand what's going on.   

`entry.S`文件中以下代码片段
``` javascript
movl	$(RELOC(entry_pgdir)), %eax
movl	%eax, %cr3
# Turn on paging.
movl	%cr0, %eax
orl	$(CR0_PE|CR0_PG|CR0_WP), %eax
movl	%eax, %cr0
```
首先设置`cr3`寄存器的值，然后将`cr0`中的`PE` `PG` `WP`位都打开，然后计算机的寻址模式进入页表翻译模式。根据`GDT`表当中的内容：  
``` javascript
(gdb) x/3x 0x7c4c
0x7c4c:	0x00000000	0x00000000	0x0000ffff
``` 
以及
```
ds: 0x10
cs: 0x8
```
可知翻译指令和数据的地址时`base address`都为0。
查看相应的地址翻译表发现地址翻译表把虚拟地址`0xf0000000~0xf0400000` 和`0x00000000~0x004000000` 映射到物理地址`0x00000000~0x00400000`

> What is the first instruction after the new mapping is established that would fail to work properly if the old mapping were still in place? Comment out or otherwise intentionally break the segmentation setup code in kern/entry.S, trace into it, and see if you were right.  

将`~/kern/entry.S`当中的  
`orl	$(CR0_PE|CR0_PG|CR0_WP), %eax`
改为  
`orl	$(CR0_PE|CR0_WP), %eax`（关掉页表翻译）
则指令
``` javascript
0x100028:	mov    $0xf010002f,%eax
0x10002d:	jmp    *%eax
```
会跳转到与正常物理地址不一样的地址，导致程序出错，自动退出。


#### Exercise 8

> We have omitted a small fragment of code - the code necessary to print octal numbers using patterns of the form "%o". Find and fill in this code fragment. Remember the octal number should begin with '0'.

代码如下：
```		
case 'o':
	// display a number in octal form and the form should begin with '0'
	putch('0', putdat);
	num = getuint(&ap, lflag);
	base = 8;
	goto number;
``` 

#### Exercise 9
> You need also to add support for the "+" flag, which forces to precede the result with a plus or minus sign (+ or -) even for positive numbers.

代码如下：
```
	case '+':
		plus = 1;
		goto reswitch;
......
    case 'd':
		num = getint(&ap, lflag);
		if ((long long) num < 0) {
			putch('-', putdat);
			num = -(long long) num;
		} else if ((long long) num >= 0 && plus){
			putch('+', putdat);
		}
		base = 10;
		goto number;
```

> Explain the interface between printf.c and console.c. Specifically, what function does console.c export? How is this function used by printf.c?  

`kern/printf.c`中函数中的`putch(int ch, int *cnt)`(作用为在当前光标处向屏幕输出字符ch，然后光标自动右移一个字符位置)调用了`kern/console.c`中的`cputchar()`。

> Explain the following from console.c:
    
```
1      if (crt_pos >= CRT_SIZE) {
2              int i;
3              memcpy(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
4              for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++)
5                      crt_buf[i] = 0x0700 | ' ';
6              crt_pos -= CRT_COLS;
7      }
```

由`#define CRT_SIZE	(CRT_ROWS * CRT_COLS)`  
可得知`CRT_SIZE`应该是控制台最大显示字体的数量，当显示字体的数量超过控制台大小时，将所有内容整体上移一行，然后将最后的内容清空，移动`crt_buf`的指针`crt_ptr`。

> For the following questions you might wish to consult the notes for Lecture 2. These notes cover GCC's calling convention on the x86.
Trace the execution of the following code step-by-step:

```
int x = 1, y = 3, z = 4;
cprintf("x %d, y %x, z %d\n", x, y, z);
```
> In the call to cprintf(), to what does fmt point? To what does ap point?  

`fmt`是地址指针，指向字符串`x %d, y %x, z %d\n`的开头。

> List (in order of execution) each call to `cons_putc`, `va_arg`, and `vcprintf`. For `cons_putc`, list its argument as well. For va_arg, list what ap points to before and after the call. For vcprintf list the values of its two arguments.

利用`gdb watch`可以跟踪`ap`指针的变化
顺序如下：
``` javascript
cprintf("x %d, y %x, z %d\n", 1, 3, 4);
vcprintf (fmt=0xf0101ad7 "x %d, y %x, z %d\n", ap=0xf010feb4 "\001")
cons_putc('x')
cons_putc(' ')
va_arg() ap: 0xf010feb4=>0xf010feb8
cons_putc('1')
cons_putc(',')
cons_putc(' ')
cons_putc('y')
cons_putc(' ')
va_arg() ap: 0xf010feb8=>0xf010febc
cons_putc('3')
cons_putc(',')
cons_putc(' ')
cons_putc('z')
cons_putc(' ')
va_arg ap: 0xf010febc=>0xf010fec0
cons_putc('4')
cons_putc('\n')
```
> Run the following code.
 ``` javascript
unsigned int i = 0x00646c72;
cprintf("H%x Wo%s", 57616, &i);
```
>What is the output? Explain how this output is arrived out in the step-by-step manner of the previous exercise. Here's an ASCII table that maps bytes to characters. 
    
输出为:`He110 World`。   
`%x`表示输出十六进制的数字，`56616`在十六进制下是`110`。在小端法中，`0x00646c72`在地址中的存储方式为：

``` javascript
            高地址
00('\0')       ^
64('d')        |
6c('l')        |
72('r')        |
            低地址
```
所以有以上输出。

> The output depends on that fact that the x86 is little-endian. If the x86 were instead big-endian what would you set `i` to in order to yield the same output? Would you need to change `57616` to a different value?  

如果是大端法，则`i`的值应该改成`0x726c6400`，`56616`不用改变。

> In the following code, what is going to be printed after `'y='`? (note: the answer is not a specific value.) Why does this happen?
    
```
cprintf("x=%d y=%d", 3);
```
输出为`x=3 y=16326828`，因为传进来只有两个参数，所以`y=%d`中应该从栈存储`3`的上方相邻的4位（未知）取出相应的值。
> Let's say that GCC changed its calling convention so that it pushed arguments on the stack in declaration order, so that the last argument is pushed last. How would you have to change cprintf or its interface so that it would still be possible to pass it a variable number of arguments?   

利用栈的数据结构，现将所有的值都读出来，然后利用`pop`操作再将相应的值输出。

#### Exercise 10
详见代码
#### Exercise 11

> Determine where the kernel initializes its stack, and exactly where in memory its stack is located. How does the kernel reserve space for its stack? And at which "end" of this reserved area is the stack pointer initialized to point to?

`entry.S`文件中：
```
# Clear the frame pointer register (EBP)
# so that once we get into debugging C code,
# stack backtraces will be terminated properly.
movl	$0x0,%ebp			# nuke frame pointer

# Set the stack pointer
movl	$(bootstacktop),%esp
```
利用`GDB` `si`调试：
``` 
0xf010002f <relocated>:	    mov    $0x0,%ebp
0xf0100034 <relocated+5>:	mov    $0xf0110000,%esp
```
可知以上指令作用为初始化栈帧。`%ebp` = `0x0`, `%esp` = `0xf0110000`。

#### Exercise 12
> To become familiar with the C calling conventions on the x86, find the address of the `test_backtrace` function in `obj/kern/kernel.asm`, set a breakpoint there, and examine what happens each time it gets called after the kernel starts. How many 32-bit words does each recursive nesting level of `test_backtrace` push on the stack, and what are those words?   
    
一层调用栈帧横跨8个32位4字节的`word`。栈帧结构如下：
``` javascript
...............
参数x
retaddr
old %ebp
xxxxxxxx(未知)
xxxxxxxx(未知)
xxxxxxxx(未知)
xxxxxxxx(未知)
被调用函数的参数x
...............
```

> The return instruction pointer typically points to the instruction after the call instruction (why?)

详见ICS
> Why can't the backtrace code detect how many arguments there actually are? How could this limitation be fixed?

因为根据原文的描述，无论有多少个参数使用，栈帧都会留出五个操作数的空间。解决这一问题，需要加入分隔符之类的字段标记处参数在栈上开始的位置。
#### Exercise 13
> Implement the backtrace function as specified above. Use the same format as in the example, since otherwise the grading script will be confused. When you think you have it working right, run make grade to see if its output conforms to what our grading script expects, and fix it if it doesn't. After you have handed in your Lab 1 code, you are welcome to change the output format of the backtrace function any way you like.

利用`read_ebp()`读取栈顶的地址，然后根据栈的结构分别读出`old ebp`, `ret addr` `args`的值，然后更新`ebp`不断循环，直到`ebp = 0`为止。  
更多细节请见代码。
#### Exercise 15
> In this exercise, you need to implement a rather easy "time" command. The output of the "time" is the running time (in clocks cycles) of the command. The usage of this command is like this: "time [command]".

在`monitor.c`当中，修改`command`映射：
```
static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Show the trace of stack", mon_backtrace },
	{ "time", "Get the CPU cycles", mon_time }
};
```
然后添加`mon_time`：
```
int 
mon_time(int argc, char **argv, struct Trapframe *tf) {
	int i = 1, size = 0, MAXLEN = 1024;
	char buf[MAXLEN];
	char *ptr = buf;

	for (; i < argc; i++) {
		char *c = argv[i];
		while (*c != '\0') {
			*(ptr++) = *c;
			c++;
			if (size++ >= MAXLEN) {
				cprintf("To many args!\n");
				return 0;
			}
		}
		*(ptr++) = ' ';
	}
	*ptr = '\0';
	unsigned long long begin, end;
	unsigned int eax, edx;
	 __asm__ volatile("rdtsc" : "=a" (eax), "=d" (edx));
	begin = ((unsigned long long)edx << 32) | (unsigned long long)eax;
	runcmd(buf, tf);
	__asm__ volatile("rdtsc" : "=a" (eax), "=d" (edx));
        end = ((unsigned long long)edx << 32) | (unsigned long long)eax;
	cprintf("kerninfo cycles: %lld\n", end - begin);	
	return 0;
}
```
首先解析出`time`指令的后面参数，然后获取初始时间，执行指令，获取结束时间，得到结果。
