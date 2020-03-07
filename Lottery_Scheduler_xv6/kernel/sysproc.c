#include "types.h"
#include "x86.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "sysfunc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;
  
  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since boot.
int
sys_uptime(void)
{
  uint xticks;
  
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

/* The following code is added/modified by your parmanand sahu(pxs180040) & Kushagra Gupta(kxg180031)
// set number of tickets for a process by passing integer from user space to kernel
*/
int
sys_settickets(void)
{
  int tickets;
  if(argint(0, &tickets) < 0)
    return -1;
return settickets(tickets);
}

// pass struct pointer to kernel for updating process table in user space
int 
sys_getpinfo(void)
{
struct pstat* process_table = NULL;
  argptr(0, (void*)&process_table, sizeof(struct pstat*));
  if (process_table == NULL) 
    return -1;
  getpinfo(process_table);
  return 0;
}

/* End of code added/modified */