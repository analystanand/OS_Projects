#include "types.h"
#include "stat.h"
#include "user.h"
#include "pstat.h"
#define check(exp, msg) if(exp) {} else {\
   printf(1, "%s:%d check (" #exp ") failed: %s\n", __FILE__, __LINE__, msg);\
   exit();}
#define tol 7

void spin()
{
	int i = 0;
  int j = 0;
  int k = 0;
	for(i = 0; i < 50; ++i)
	{
		for(j = 0; j < 400000; ++j)
		{
			k = j % 10;
      k = k + 1;
    }
	}
}

void print(struct pstat *st)
{
   int i;
   for(i = 2; i < NPROC; i++) { //skip init and sh
      if (st->inuse[i]) {
          printf(1, "pid: %d tickets: %d ticks: %d\n", st->pid[i], st->tickets[i], st->ticks[i]);
      }
   }
}

int
main(int argc, char *argv[])
{
   int pid_chds[2];
   int i = 0;
   pid_chds[0] = getpid();
   check(settickets(100) == 0, "settickets");
   if((pid_chds[1]= fork())>0) // parent
        {    
        if((pid_chds[2]= fork())>0)// parent
            { 
            spin();}
            
        else { //child2
          check(settickets(200) == 0, "settickets");
           for (;;){spin();}
             }
        }
   else
   { //child1
   check(settickets(300) == 0, "settickets");
     for (;;){spin();}
   }
   
   struct pstat st_before;
  int dummy_tick = 1;
  new:
  if (getpinfo(&st_before) != 0){
     printf(1,"check failed: getpinfo\n");
     goto Cleanup;
   }
   spin();

   printf(1, "\n---------------------------------------\n");
   printf(1,"Dummy Tick %d\n",dummy_tick);
   print(&st_before);
   dummy_tick++;
   goto new;
   goto Cleanup;
	
   
Cleanup:
   for (i = 0; pid_chds[i] > 0; i++){
     kill(pid_chds[i]);
   }

  while(wait() > -1);

     exit();
}