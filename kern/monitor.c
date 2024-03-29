// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line

static int runcmd(char *buf, struct Trapframe *tf);

struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Show the trace of stack", mon_backtrace },
	{ "time", "Get the CPU cycles", mon_time }
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

unsigned read_eip();

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		(end-entry+1023)/1024);
	return 0;
}

// Lab1 only
// read the pointer to the retaddr on the stack
static uint32_t
read_pretaddr() {
    uint32_t pretaddr;
    __asm __volatile("leal 4(%%ebp), %0" : "=r" (pretaddr)); 
    return pretaddr;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	cprintf("Stack backtrace:\n");
	uint32_t *ebp = (uint32_t *)read_ebp();
	while (ebp) {	
		cprintf("  eip %08x ebp %08x args %08x %08x %08x %08x %08x\n",
			ebp[1], (uint32_t)ebp, ebp[2], ebp[3], 
			ebp[4], ebp[5], ebp[6]);
		struct Eipdebuginfo info;
		debuginfo_eip(ebp[1], &info);
		cprintf("     %s:%d: %.*s+%d\n", info.eip_file, info.eip_line, 
		info.eip_fn_namelen ,info.eip_fn_name, ebp[1] - info.eip_fn_addr);
		
		ebp = (uint32_t *)ebp[0];
	
	
	}
	cprintf("Backtrace success\n");
	return 0;
}

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


/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}

// return EIP of caller.
// does not work if inlined.
// putting at the end of the file seems to prevent inlining.
unsigned
read_eip()
{
	uint32_t callerpc;
	__asm __volatile("movl 4(%%ebp), %0" : "=r" (callerpc));
	return callerpc;
}
