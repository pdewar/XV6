#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

/////////////////////
//cs 153 lab 2
uint sp;
pde_t *pgdir;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{

  //cs 153 lab 2
  //cprintf("\n\n//////////////////////////////////////////////////\n");
  //cprintf("Enter trap() in trap.c...\n\n");

  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //////////////////////
  //cs 153 lab 2
  case T_PGFLT:
    
    //cprintf("\n\nrcr3(): %d\n\n", rcr3());    

    //*pgdir = rcr3(); // gets the address of the page directory

    //cprintf("\n\nEntered trap T_PGFLT...\n\n");

    //cprintf("tr->esp: %d\n\n", tf->esp);

    //cprintf("sp before assignment: %d\n\n", sp);
    
    //sp = PGROUNDUP(STACKBASE - myproc()->numStackPages*PGSIZE);
    //sp = STACKBASE - (myproc()->numStackPages*PGSIZE + 4);
    //sp = STACKBASE - myproc()->numStackPages*PGSIZE + 4;
    sp = tf->esp; // gets the stack pointer from the trap frame    
    cprintf("PGROUNDOWN(SP)%d\n",PGROUNDDOWN(sp));
    cprintf("STACKBASE: %d\n", STACKBASE);
    cprintf("sp (after assignment): %d\n", sp);
    cprintf("rcr2(): %d\n\n", rcr2());
    
    //cprintf("myproc(): %d\n", myproc());
    //cprintf("STACKBASE: %d\n", STACKBASE);
    //cprintf("myproc()->numStackPages: %d\n", myproc()->numStackPages);
    //cprintf("myproc()->numStackPages*PGSIZE + 4: %d\n\n", myproc()->numStackPages*PGSIZE + 4);

    //cprintf("myproc()->pgdir: %d\n", myproc()->pgdir);
    
    //(allocuvm(myproc()->pgdir, myproc()->stack_addr-PGSIZE, PGROUNDDOWN(sp)) == 0)
    if( (rcr2() < sp) && (rcr2() > (sp-PGSIZE) ) ) {
      if(allocuvm(myproc()->pgdir, (PGROUNDDOWN(sp)-PGSIZE), PGROUNDDOWN(sp)) == 0) {
        cprintf("\n\nno more memory to allocate, exiting program...\n\n");
        myproc()->numStackPages = (myproc()->numStackPages)+1;
	//exit();
      }
    }

    //cprintf("\n\nafter if statement for allocuvm in trap(), about to leave trap...\n\n");
    
    break;
  //////////////////////
  

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
