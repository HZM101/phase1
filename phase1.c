/* ------------------------------------------------------------------------
   phase1.c

   CSCV 452

   @authors: Erik Ibarra Hurtado, Hassan Martinez, Victor Alvarez

   ------------------------------------------------------------------------ */
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <phase1.h>
#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *dummy);
extern int start1 (char *);
void dispatcher(void);
void launch();
static void enableInterrupts();
static void check_deadlock();
void dump_processes(void);
static void insertRL(proc_ptr);
int zap(int);
int is_zapped(void);
void de_zap(void);
static void removeFromRL(int);
extern void insert_child(proc_ptr);
int block_me(int);
int unblock_proc(int);
int readtime(void);
void clock_handler(int, void *);
void mode_checker();

/* -------------------------- Globals ------------------------------------- */

/* Patrick's debugging global variable... */
int debugflag = 1;

/* the process table */
proc_struct ProcTable[MAXPROC];

/* Process lists  */
/* ReadyList is a linked list of process pointers */
proc_ptr ReadyList = NULL;

/* current process ID */
proc_ptr Current;

/* the next pid to be assigned */
unsigned int next_pid = SENTINELPID;

/* empty proc_struct */
proc_struct empty_struct = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0};

/* define the variable for the interrupt vector declared by USLOSS */
void(*int_vec[NUM_INTS])(int dev, void * unit);

/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
	     Start up sentinel process and the test process.
   Parameters - none, called by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup()
{
   int i;      /* loop index */
   int result; /* value returned by call to fork1() */

   /* initialize the process table */
   for( i = 0; i < MAXPROC; i++)
   {
      console("startup(): initializing process table, Proctable[]\n");
      ProcTable[i] = empty_struct;
   }

   /* Initialize the Ready list, etc. */
   if (DEBUG && debugflag)
      console("startup(): initializing the Ready & Blocked lists\n");

   /* Initialize the clock interrupt handler */
   int_vec[CLOCK_DEV] = clock_handler;

   /* startup a sentinel process */
   if (DEBUG && debugflag)
       console("startup(): calling fork1() for sentinel\n");
   result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK, 
                  SENTINELPRIORITY);
   if (result < 0) {
      if (DEBUG && debugflag)
         console("startup(): fork1 of sentinel returned error, halting...\nResult = %d\n", result);
      halt(1);
   }

   /* start the test process */
   if (DEBUG && debugflag)
      console("startup(): calling fork1() for start1\n");
   result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
   if (result < 0) {
      console("startup(): fork1 for start1 returned an error, halting...\n");
      halt(1);
   }

   console("startup(): Should not see this message! ");
   console("Returned from fork1 call that created start1\n");

   return;
} /* startup */


/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish()
{
   if (DEBUG && debugflag)
      console("in finish...\n");
} /* finish */


/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int(*f)(char *), char *arg, int stacksize, int priority)
{
   /* set to 0 for searching an empty slot in the process table */
   int proc_slot = 0;

   if (DEBUG && debugflag)
      console("fork1(): creating process %s\n", name);

   /* test if in kernel mode; halt if in user mode */
   mode_checker("fork1()");

   /* Return if stack size is too small */
   if (stacksize < USLOSS_MIN_STACK)
   {
      console("fork1(): Stack size is too small.\n");
      return (-2);
   }

   /* create a stack pointer */
   char* stack_ptr = (char*) malloc (stacksize * sizeof(int));

   /* find an empty slot in the process table */
   while (ProcTable[proc_slot].pid != NULL)
   {
      proc_slot++;
      if (proc_slot == MAXPROC)
      {
         console("fork1(): no empty slots in the process table.");
         return -1;
      }
   }

   /* fill-in entry in process table */
   if ( strlen(name) >= (MAXNAME - 1) ) {
      console("fork1(): Process name is too long.  Halting...\n");
      halt(1);
   }
   strcpy(ProcTable[proc_slot].name, name);

   /* process starting function */
   ProcTable[proc_slot].start_func = f;

   /* process function argument */
   if ( arg == NULL )
      ProcTable[proc_slot].start_arg[0] = '\0';
   else if ( strlen(arg) >= (MAXARG - 1) ) {
      console("fork1(): argument too long.  Halting...\n");
      halt(1);
   }
   else
      strcpy(ProcTable[proc_slot].start_arg, arg);
   
   /* process stack pointer */
   ProcTable[proc_slot].stack = stack_ptr;

   /* process stacksize */
   ProcTable[proc_slot].stacksize = stacksize;

   /* process pid */
   ProcTable[proc_slot].pid = next_pid++;

   /* if priority is out-of-range */
   if (priority < HIGHEST_PRIORITY || priority > LOWEST_PRIORITY)
   {
      return (-1);
   }

   /* process priority */
   ProcTable[proc_slot].priority = priority;

   /* process status (READY by default) */
   ProcTable[proc_slot].status = READY;

   /* if Current is a Parent process, insert the child link & add to num_kids. */
   if (Current != NULL)
   {
      insert_child(&ProcTable[proc_slot]); 
      Current->num_kids ++;                
   }

   /* Point to process in the ReadyList */
   insertRL(&ProcTable[proc_slot]);

   /* Initialize context for this process, but use launch function pointer for
    * the initial value of the process's program counter (PC)
    */
   context_init(&(ProcTable[proc_slot].state), psr_get(),
                ProcTable[proc_slot].stack, 
                ProcTable[proc_slot].stacksize, launch);

   /* call dispatcher - exception for sentinel */
   if (strcmp(ProcTable[proc_slot].name, "sentinel") != 0)
   {
      console("fork1(): calling dispatcher\n");
      dispatcher();
   }
   
   /* for future phase(s) */
   p1_fork(ProcTable[proc_slot].pid);

   /* Return PID of created process */
   return (ProcTable[proc_slot].pid);

} /* fork1 */


/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
   int result;

   if (DEBUG && debugflag)
      console("launch(): started\n");

   /* Enable interrupts */
   enableInterrupts();

   /* Call the function passed to fork1, and capture its return value */
   result = Current->start_func(Current->start_arg);

   if (DEBUG && debugflag)
      console("Process %d returned to launch\n", Current->pid);

   quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If 
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the 
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
		-1 if the process was zapped in the join
		-2 if the process has no children
   Side Effects - If no child process has quit before join is called, the 
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *status)
{   
   /* Process does not have any children while a zombie block could be NULL */
   if(Current->child_proc_ptr == NULL || Current->num_kids == 0)
   {
      return -2;  
   }

   /* Current process has called join so needs to be blocked until child process quits. */
   Current->status = BLOCKED;

   /* Ensuring the status is BLOCKED to call dispatcher. */
   if(Current->status == BLOCKED)
   {
      console("join(): calling dispatcher\n");
      dispatcher();
   }

   /* Process is zapped while waiting for child to quit. */
   if (Current->is_zapped == ZAPPED)
   {
      return -1;
   }

   /* Save child process that quit to *status of parent that call join. */
   *status = Current->exit_code;   

   /* Return the PID of the child process that quit. */
   return Current->child_proc_ptr->pid;

} /* join */


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int code)
{
   console("Quitting for process %s\n", Current->name);
   mode_checker("quit");

   disableInterrupts();


   // printf("Current Stack %s\n", Current->stack);
   // free(Current->stack);

   // Current->stack = NULL;
   // Current->stacksize = 0;
   

   // Current->name = '\0';
   // Current->start_arg = '\0';
   Current->start_func = NULL;
   Current->status = QUIT;
   Current->exit_code = code;

   // console("Reached\n");
   // console("Freed current stack \n");
    
   // dump_processes();

   if(Current->num_kids == 0) {
      Current->child_proc_ptr = NULL;
   }
   if (Current->child_proc_ptr != NULL)
   {
      console("Number of kids %d\n", Current->num_kids);
      console("quit(): process %d has active children.\nHalting..\n", Current->pid);
      halt(1);
   }

   // if Current process is sentinel or start1, then they don't have a parent.
   if(Current->pid == 1 || Current->pid == 2) {
      // no parent has to join
      Current->status == NO_CURRENT_PROCESS;

      // if current process is startup
      // Clean up since no more joining is happening
      if (Current->pid == 2)
      {
         Current->child_proc_ptr = NULL;
         Current->exit_code = 0;
         Current->pid = -1;
         Current->start_func = NO_CURRENT_PROCESS;
         // Current->parent_ptr->pid = -1; 
         Current->priority = -1;

         Current = NULL;

         de_zap();
         dispatcher();

         console("quit(): This shouldn't be seen.");
         halt(1);
      }
   }
   else {
      console("Current pid is %d\n", Current->pid);
      // removeFromRL(Current->pid);
      console("Else reach\n");
      Current->status = QUIT;

      int proc_slot = 0;

      console("remove from Rl\n");
      // removing process from readyList
      

      console("remove reach\n");
     
      Current->parent_ptr->num_kids--;
      if(Current->parent_ptr->num_kids == 0) {
         console("Parent has zero children\n");
         // Current->parent_ptr->child_proc_ptr = NULL;
         // crucial line of code
         Current->parent_ptr->child_proc_ptr->exit_code = code;
         Current->parent_ptr->child_proc_ptr->pid = Current->pid;
         
         
      }


      Current->parent_ptr->child_proc_ptr->exit_code = code;
      Current->parent_ptr->child_proc_ptr->pid = Current->pid;

      Current->parent_ptr->status = READY;
      insertRL(Current->parent_ptr);
            
      p1_quit(Current->pid);


      // free(Current->stack);

      Current->stack = NULL;
      Current->stacksize = 0;
      // Current->status = EMPTY;
     
      

      // Where do we free memory ?????
      
      // int proc_slot1 = 0;

   
      // while (ProcTable[proc_slot1].pid != Current->pid)
      // {
      //    proc_slot1++;
      //    if (proc_slot1 == MAXPROC)
      //    {
      //       console("p1_quit(): no pid in the process table.");
      //    }
      // }
      
      // ProcTable[proc_slot1].pid = NULL;
      // free(ProcTable[proc_slot1].stack);

   // Current->stack = NULL;
   // Current->stacksize = 0;
      
      de_zap();
      printf("Current exit status for process %s is %d\n", Current->name, Current->exit_code);
      dispatcher();
   } 
     
 
   // // if function returns successfully
   // if (code == 0) {
   //    if (Current->num_kids == 0 ) {  
   //       // console("number of kids 0;\n");
   //       if(Current->parent_ptr != NULL) {

   //          int proc_slot = 0;

   //          if(DEBUG && debugflag)
   //          {
   //             console("quit() called: pid = %d\n", Current->pid);
   //          }

   //          while (ProcTable[proc_slot].pid != Current->pid)
   //          {
   //             proc_slot++;
   //             if (proc_slot == MAXPROC)
   //             {
   //                console("p1_quit(): no pid in the process table.");
   //             }
   //          }
     
   //          printf("Current pid = %d\n", Current->pid);
   //          //    ProcTable[proc_slot].pid;

   //          printf( "FOUND pid = %d\n", ProcTable[proc_slot].pid);

   //          // updating proc_table
   //          console("Parent = %s, %s, %d, %d, is zapped? = %d\n", Current->parent_ptr->name,Current->name, Current->pid, Current->priority, Current->is_zapped);
            
   //          // while(proc_slot < MAXPROC) {
   //          //    ProcTable[proc_slot] = ProcTable[proc_slot + 1];
   //          //    proc_slot++;
               
   //          // } 
            
   //          printf("Current pid after fixing table = %d\n", Current->pid);


   //          // console("Status of quit %s\n", ProcTable[proc_slot].status);

            
   //          Current->status = QUIT;
   //          Current->exit_code = -getpid();
            
   //          Current->parent_ptr->status = READY;
   //          // Current->parent_ptr->num_kids--;
            
            
   //          if(Current->parent_ptr->num_kids > 0) {
   //             Current->parent_ptr->num_kids--;
   //             Current->parent_ptr->child_proc_ptr->pid = Current->pid;
   //          }
            
   //          // dump_processes();
   //          if(Current->status == QUIT)
   //          {
   //             console("calling dispatcher after quiting process\n");
   //             insertRL(Current->parent_ptr);
   //             // removeFromRL(Current);
   //             dispatcher();
   //             // p1_quit(Current->pid);
   //          }


   //       }
   //       else if(Current->parent_ptr == NULL) {
   //          Current->status = QUIT;
   //          // Current->exit_code = 0;
           
          
   //          // dump_processes();
   //          if(Current->status == QUIT)
   //          {
   //             // console("calling dispatcher after quiting process\n");
           
   //             dispatcher();
   //          }
   //       }
            
   //    }
      
   // }
   // else {
   //    printf("Current function fails to return");

   // }
   
   
   
   // this quit kill a process.
   // p1_quit(Current->pid);
} /* quit */


/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)
{

   /* Check priority under the time limit for current process to continue to run. */  
   if(Current != NULL && Current->priority <= ReadyList->priority && Current->status == RUNNING && readtime() < 80)
   {
      return;
   }

   proc_ptr next_process;
   proc_ptr old_process;

   next_process = ReadyList;
   old_process = Current;
   Current = next_process;

   /* Checking old_process if is NULL so the next_process can RUN. */
   if (old_process == NULL)
   {
      next_process->status = RUNNING;
      removeFromRL(next_process->pid);
      next_process->start_time = sys_clock();
      context_switch(NULL, &next_process->state);
   }
   /* Not NULL but has QUIT allow next_process to RUN also.*/
   else if (old_process->status == QUIT)
   {
      next_process->status = RUNNING;
      removeFromRL(next_process->pid);
      /* Get time spent in porcessor for old_process and update pc_time. */
      old_process->pc_time = old_process->pc_time + readtime();
      next_process->start_time = sys_clock();
      context_switch(&old_process->state, &next_process->state);
   }
   /* Otherwise move to the next_process. */
   else
   {
      next_process->status = RUNNING;
      removeFromRL(next_process->pid);

      /* if the "running" process is not-blocked, insert it into the ready list. */
      if (old_process->status != BLOCKED)
      {
         old_process->status = READY;
         insertRL(old_process);
         console("\n");
      }

      /* Get time spent in porcessor for old_process and update pc_time. */
      old_process->pc_time = old_process->pc_time + readtime();
      next_process->start_time = sys_clock(); 
      context_switch(&old_process->state, &next_process->state);
   }
   
} /* dispatcher */


/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
	     processes are blocked.  The other is to detect and report
	     simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
		   and halt.
   ----------------------------------------------------------------------- */
int sentinel (char *dummy)
{
   if (DEBUG && debugflag)
      console("sentinel(): called\n");
   while (1)
   {
      check_deadlock();
      waitint();
   }
} /* sentinel */


/* check to determine if deadlock has occurred... */
static void check_deadlock()
{

   /* Check PCB if any processes are active. */
   for( int i = 0; i < MAXPROC; i++)
   {
      if (ProcTable[i].pid != NULL)
      {
         /* If processes remain then termination of USLOSS - halt(1). */
         if (ProcTable[i].pid != SENTINELPID && ProcTable[i].status != QUIT)
         {
            console("Processes: %s = %d (abnormal termination USLOSS - halt(1))\n", ProcTable[i].name, ProcTable[i].status);
            halt(1);
         }
      }
   }

   /* Terminate the simulation with normal message. */
   console("All processes completed USLOSS - halt(0)\n");
   halt(0);
} /* check_deadlock */


/* Disables the interrupts. */
void disableInterrupts()
{
   mode_checker("disableInterrupts()");
   /* We ARE in kernel mode */
   psr_set( psr_get() & ~PSR_CURRENT_INT );
} /* disableInterrupts */


/* Enables the interrupts. */
static void enableInterrupts()
{
   psr_set((psr_get() | PSR_CURRENT_INT));
} /*enableInterrupts*/


/* ---------------------------------------------------------------------------------
   Name - clock_handler
   Purpose - check if current process has exceeded its time slice then calls dispatcher.
   ---------------------------------------------------------------------------------*/
void clock_handler(int dev, void *unit)
{
   if (readtime() >= 80)
   {
      console("clock_handler(): calling dispatcher().");
      dispatcher();
   }
   return;
} /* clock_handler */


/* ---------------------------------------------------------------------------------
   Name - zap
   Purpose - a process arranges for another process to be killed by calling zap.
   ---------------------------------------------------------------------------------*/
int zap(int pid)
{

   int proc_slot = 0;

   proc_ptr walker;

   /* Searching PID to be zap. */
   while(ProcTable[proc_slot].pid != pid)
   {
      proc_slot++;
      /* If it reaches MAXPROC PID does not exist. */
      if(proc_slot == MAXPROC)
      {
         console("zap(): Process does not exist\n");
         halt(1);
      }
   }

   /* If PID its the same to the Current PID */
   if(ProcTable[proc_slot].pid == Current->pid)
   {
      console("zap(): Process tried to zap itself.\n");
      halt(1);
   }

   /* Process in is_zapped is set to ZAPPED. */
   ProcTable[proc_slot].is_zapped = ZAPPED;

   /* Creating linked list of the zapper. 
   if(ProcTable[proc_slot]->zapped_by_ptr == NULL)
   {
      ProcTable[proc_slot]-> zapped_by_ptr = Current;
   }
   else
   {
      walker = ProcTable[proc_slot]->zapped_by_ptr;
      while(walker->next_zapper_ptr != NULL)
      {
         walker = walker->next_zapper_ptr;
      }
      walker->next_zapper_ptr = Current;
   }
*/
   /* Blocking the process that call zap. */
   Current->status = BLOCKED;

   /* Zapped process called quit. */
   if(ProcTable[proc_slot].status == QUIT){return 0;}

   /* Calling dispatcher(); */
   console("zap(): calling dispatcher\n");
   dispatcher();

   /* If the zapped process happen while in zap function. */
   if(Current->is_zapped == ZAPPED)
   {
      console("zap(): This process was zapped while in the zap function.");
      return -1;
   }

   /* Zapped process called quit. */
   if(ProcTable[proc_slot].status == QUIT){return 0;}

   return 0;
} /* zap */


/* ---------------------------------------------------------------------------------
   Name - is_zapped
   Purpose - Checks if the current process is_zapped or not.
   ---------------------------------------------------------------------------------*/
int is_zapped(void)
{
   if(Current->is_zapped == ZAPPED)
   {
      return ZAPPED;
   }
   else
   {
      return NOT_ZAPPED;
   }
} /* is_zapped */

/* ---------------------------------------------------------------------------------
   Name - de_zap
   Purpose - Unblock all zapped process.
   ---------------------------------------------------------------------------------*/
void de_zap(void)
{
   proc_ptr walker;
   proc_ptr previous;

   /* Quit if it is already clean. */
   if(Current->zapped_by_ptr == NULL)
   {
      return;
   }
   else
   {
      /* Pointing to de_zap processes. */
      walker = Current->zapped_by_ptr;
      Current->zapped_by_ptr = NULL;

      /* Not NULL then needs to be clean. */
      while(walker->next_zapper_ptr != NULL)
      {
         /* Setting walker & previous to move around the list. */
         previous = walker;
         walker = walker->next_zapper_ptr;

         /* Cleanning the process. */
         previous->next_zapper_ptr = NULL;
         /* Setting ready from cleanning. */
         previous->status = READY;
         /* Additing to the RL. */
         insertRL(previous);
      }
   }

   /* Final cleaning. */
   walker->status = READY;
   insertRL(walker);

   return;
} /* de_zap */


/* ------------------------------------------------------------------------------
   Name - dump_processes
   Purpose - Prints process information to the console.
   ------------------------------------------------------------------------------*/
void dump_processes(void)
{

   console("\n-----------------------------------------dump_processes-----------------------------------------\n");

   for(int i = 0; i < MAXPROC; i++)
   {
      console("Entry %d:\n", i);
      console("Name: %s\n", ProcTable[i].name);
      console("PID: %d\n", ProcTable[i].pid);

      if(ProcTable[i].parent_ptr == NULL)
      {
         console("Parent PID: N/A\n");
      }
      else
      {
         console("Parent PID: %d\n", ProcTable[i].parent_ptr->pid);
      }
      
      console("# of Children: %d\n", ProcTable[i].num_kids);
      console("CPU time: %d ms\n", ProcTable[i].start_time); 

      switch(ProcTable[i].status)
      {
         case READY:
            console("Status: READY\n");
            break;
         case BLOCKED:
            console("Status: BLOCKED\n");
            break;
         case RUNNING:
            console("Status: RUNNING");
            break;
         case QUIT:
            console("Status: QUIT\n");
            break;
         default:
            console("Status: QUIT 'DEFAULT'\n");
      }
   }
} /* dump_processes */


/* -------------------------------------------------------------------------------
   Name - insertRL
   Purpose - inserts entries into the ReadyList in appropriate order by priority
   Parameters - a process pointer to a PCB block
   -------------------------------------------------------------------------------*/
static void insertRL(proc_ptr proc)
{
   proc_ptr walker, previous;  //pointers to PCB
   previous = NULL;
   walker = ReadyList;
   while (walker != NULL && walker->priority <= proc->priority) {
      previous = walker;
      walker = walker->next_proc_ptr;
   }
   if (previous == NULL) {
      /* process goes at front of ReadyList */
      proc->next_proc_ptr = ReadyList;
      ReadyList = proc;
   }
   else {
      /* process goes after previous */
      previous->next_proc_ptr = proc;
      proc->next_proc_ptr = walker;
   }
   return;
} /* insertRL */


/* --------------------------------------------------------------------------------
   Name - removeFromRL
   Purpose - removes entry from the ReadyList
   Parameters - Accepts a PID of the process to be removed
   --------------------------------------------------------------------------------*/
static void removeFromRL(int PID)
{
   proc_ptr walker, tmp;
   walker = ReadyList;
   tmp = walker;

   /* if sentinel is the only process in the ready list, don't do anything */
   if(walker->next_proc_ptr == NULL)
   {
      return;
   }

   /* if process is the first item in the ready list */
   if(walker->pid == PID)
   {
      ReadyList = walker->next_proc_ptr;
      walker->next_proc_ptr = NULL;
      return;
   }

   /* if process is somewhere else in the ready list, walk through the list */
   while (walker != NULL && walker->pid != PID)
   {
      tmp = walker;
      walker = walker->next_proc_ptr;
   }
   if (walker->pid == PID)
   {
      tmp->next_proc_ptr = walker->next_proc_ptr;
      walker->next_proc_ptr = NULL;
      return;
   }
   return;
} /* removeFromRL */


/* ------------------------------------------------------------------------------------
   Name - insert_child
   Purpose - inserts a child process into the list.
   ------------------------------------------------------------------------------------*/
void insert_child(proc_ptr child)
{
   /* Family tree (walker) if their is no empty space for child. */
   proc_ptr walker;

   /* Check for empty space to insert child. */
   if(Current->child_proc_ptr == NULL)
   {
      Current->child_proc_ptr = child;
   }
   /* else move to lower list. */
   else
   {
      walker = Current->child_proc_ptr;
      while(walker->next_sibling_ptr != NULL)
      {
         walker = walker->next_sibling_ptr;
      }
      walker->next_sibling_ptr = child;
   }
   
   /* child becomes parent to current. */
   child->parent_ptr = Current;
} /* insert_child */


/* Blocks the calling process. */
int block_me(int new_status)
{
   /* If new_status is lower or = to 10 halt USLOSS with error message. */
   if (new_status <= 10)
   {
      console("block_me(): -ERROR CODE- new_status <= 10. Halt(1)\n");
      halt(1);
   }

   /* If process was zap while blocked return -1. */
   if (Current->is_zapped == ZAPPED)
   {
      return -1;
   }

   /* Normal block the calling process. */
   Current->status = BLOCKED;
   Current->blocked_status = new_status;
   return 0;
} /* block_me */


/* -------------------------------------------------------------------------------
   Name - unblock_proc
   Purpose - unblocks process with pid that had been blocked by calling block_me().
   -------------------------------------------------------------------------------*/
int unblock_proc(int pid)
{
   /* return -1 if the calling process was zapped. */
   if (Current->is_zapped == ZAPPED)
   {
      return -1;
   }

   int i = 0;
   for(i = 0; i < MAXPROC; i++)
   {
      if(ProcTable[i].pid == pid || i == MAXPROC)
      {
         /* return -2 under this conditions. */
         if (ProcTable[i].status != BLOCKED || 
            ProcTable[i].pid == Current->pid || 
            ProcTable[i].blocked_status <= 10)
         {
            return -2;
         }
         break;
      }
   }

   ProcTable[i].status = READY;
   insertRL(&ProcTable[i]);
   dispatcher();

   /* return 0 if unblock is sucessful. */
   return 0;
} /* unblock_proc */


/* -------------------------------------------------------------------------------
   Name - readtime
   Purpose - returns CPU time (in milliseconds) used by the current process.
   -------------------------------------------------------------------------------*/
int readtime(void)
{
   return (sys_clock() - Current->start_time) / 1000;
} /* readtime */

/* ---------------------------------------------------------------------------------
   Name - mode_checker
   Purpose - Check the mode if mode is in user mode halt(1).
   ---------------------------------------------------------------------------------*/
void mode_checker(char *func_name)
{
   if((PSR_CURRENT_MODE & psr_get()) == 0)
   {
      console("%s: called while in user mode, by process %d. halt(1)...\n", func_name, Current->pid);
      halt(1);
   }
} /* mode_checker */

