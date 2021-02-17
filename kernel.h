#define DEBUG 0

typedef struct proc_struct proc_struct;

typedef struct proc_struct * proc_ptr;

struct proc_struct {
   proc_ptr       next_proc_ptr;
   proc_ptr       child_proc_ptr;
   proc_ptr       next_sibling_ptr;
   proc_ptr       parent_ptr;
   char           name[MAXNAME];     /* process's name */
   char           start_arg[MAXARG]; /* args passed to process */
   context        state;             /* current context for process */
   short          pid;               /* process id */
   int            priority;
   int (* start_func) (char *);      /* function where process begins -- launch */
   char          *stack;
   unsigned int   stacksize;
   int            status;            /* RUNNING, READY, BLOCKED, QUIT, etc. */
   int            is_zapped;         /* ZAPPED, NOT_ZAPPED */
   proc_ptr       zapped_by_ptr;
   proc_ptr       next_zapper_ptr;
   int            exit_code;         /* exit code of process when it calls quit */
   int            blocked_status;    /* indicates how something was blocked */
   int            start_time;        /* records the start time in microseconds */
   int            num_kids;          /* keeps count of number of children process has */
   int            pc_time;           /* running total amount of  pc_time process has had in processor */
   /* other fields as needed... */
};

struct psr_bits {
         unsigned int cur_mode:1;
       	unsigned int cur_int_enable:1;
         unsigned int prev_mode:1;
         unsigned int prev_int_enable:1;
    	   unsigned int unused:28;
};

union psr_values {
   struct psr_bits bits;
   unsigned int integer_part;
};

/* Some useful constants.  Add more as needed... */
#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY LOWEST_PRIORITY
#define RUNNING 0
#define READY 1
#define BLOCKED 2
#define QUIT 3
#define NOT_ZAPPED 0
#define ZAPPED 1

