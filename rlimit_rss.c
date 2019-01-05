

#include <linux/module.h>
#include <linux/sched/signal.h>


MODULE_DESCRIPTION(  "Enforces RLIMIT_RSS"            );
MODULE_VERSION(      "0.0-devel-20190105"             );
MODULE_AUTHOR(       "Parke <parke.nexus@gmail.com>"  );
MODULE_LICENSE(      "GPL"    /* GPLv2 */             );




/*  process  ----------------------------------------------------  process  */


typedef void  process_handler  ( struct task_struct * const pr );


static void  apply_to_all  ( process_handler fn )  {    /*  ---  apply ...  */
  struct task_struct *  pr;
  rcu_read_lock();
  list_for_each_entry_rcu( pr, & init_task .tasks, tasks )  {  fn ( pr );  }
  rcu_read_unlock();  }


struct  process_info  {    /*  ---------------------  struct  process_info  */
  struct task_struct *  process;
  long                  pid;
  unsigned long         rss;
  unsigned long         hard;
  unsigned long         soft;  };


static unsigned long  rss_calc    /*  --------------------------  rss_calc  */
( const struct mm_struct * const  mm )  {
  const atomic_long_t * const  c          =  mm -> rss_stat .count;
  const unsigned long          rss_pages  =
    c [ MM_FILEPAGES ] .counter  +  c [ MM_ANONPAGES ] .counter;
  return  rss_pages << PAGE_SHIFT;  }    /*  convert pages to bytes  */


static void  info_lookup    /*  -----------------------------  info_lookup  */
( struct task_struct  * const  pr,
  struct process_info * const  info )  {
  /*  see  https://www.kernel.org/doc/html/latest/vm/active_mm.html  */
  const struct signal_struct * const  sig   =  pr ?  pr -> signal  :  NULL  ;
  const struct mm_struct     * const  mm    =  pr ?  pr -> mm      :  NULL  ;
  const struct rlimit        * const  rlim  =
    sig  ?  & sig -> rlim [ RLIMIT_RSS ]  :  NULL  ;
  info -> process  =  pr;
  info -> pid      =  pr    ?  pr -> pid         :  -1             ;
  info -> rss      =  mm    ?  rss_calc ( mm )   :  0              ;
  info -> hard     =  rlim  ?  rlim -> rlim_max  :  RLIM_INFINITY  ;
  info -> soft     =  rlim  ?  rlim -> rlim_cur  :  RLIM_INFINITY  ;  }


static void  kill  ( const struct process_info * const p )  {    /*  -----  */
  printk ( KERN_INFO
	   "rlimit_rss  kill  pid %ld  rss %lu  hard %lu  soft %lu\n",
	   p -> pid, p -> rss, p -> hard, p -> soft );
  /*  see  man 9 trace_signal_generate  */
  send_sig_info ( SIGKILL, SEND_SIG_PRIV, p -> process );  }


static int  too_big  ( unsigned long actual,    /*  -------------  too_big  */
		       unsigned long limit )  {
  return  limit != RLIM_INFINITY  &&  actual > limit;  }


static void  scan_process  ( struct task_struct * const pr )  {    /*  ---  */
  struct process_info  v;  info_lookup ( pr, & v );
  if       ( too_big ( v.rss, v.hard ) )  {  kill ( & v );  }
  else if  ( too_big ( v.rss, v.soft ) )  {  kill ( & v );  }  }


/*  end  process  ------------------------------------------  end  process  */




/*  census  ------------------------------------------------------  census  */


static unsigned int  pop, pop_hinf, pop_sinf;    /*  process counts  */


static void  census_reset  ( void )  {    /*  --------------  census_reset  */
  pop  =  pop_hinf  =  pop_sinf  =  0;  }


static void  census_count  ( struct task_struct * const pr )  {    /*  ---  */
  struct process_info  v;  info_lookup ( pr, & v );
  pop ++;
  if  ( v.hard == RLIM_INFINITY )  {  pop_hinf ++;  }
  if  ( v.soft == RLIM_INFINITY )  {  pop_sinf ++;  }  }


static void  census_find  ( struct task_struct * const pr )  {    /*  ----  */
  struct process_info  v;  info_lookup ( pr, & v );
  if  (  v.hard < RLIM_INFINITY  ||  v.soft < RLIM_INFINITY  )  {
    printk ( KERN_INFO
	     "rlimit_rss  found  pid %ld  rss %lu  hard %lu  soft %lu\n",
	     v.pid, v.rss, v.hard, v.soft );  }  }


static void  census_log  ( void )  {    /*  ------------------  census_log  */
  census_reset();  apply_to_all ( census_count );
  printk ( KERN_INFO "rlimit_rss  tasks %u  hard %u  soft %u\n",
	   pop,  pop - pop_hinf,  pop - pop_sinf );
  apply_to_all ( census_find );  }


/*  end  census  --------------------------------------------  end  census  */




/*  main  ----------------------------------------------------------  main  */


static int                        active     =  1;
static struct workqueue_struct *  workqueue  =  NULL;

static void  pulse  ( struct work_struct * p );
static DECLARE_DELAYED_WORK( Task, pulse );


static void  pulse  ( struct work_struct * p )  {    /*  ----------  pulse  */
  apply_to_all ( scan_process );
  if  ( active )  {  queue_delayed_work ( workqueue, & Task, 10 );  }  }


int __init  init_module  ( void )  {    /*  -----------------  init_module  */
  const char * const  queue_name  =  "rlimit_rss.c";
  printk ( KERN_INFO "rlimit_rss  init\n" );
  census_log();
  workqueue  =  create_workqueue ( queue_name );
  queue_delayed_work ( workqueue, & Task, 1 );
  return  0;  }


void __exit  cleanup_module  ( void )  {    /*  ----------  cleanup_module  */
  active  =  0;
  cancel_delayed_work ( & Task    );
  flush_workqueue     ( workqueue );
  destroy_workqueue   ( workqueue );
  census_log();
  printk ( KERN_INFO "rlimit_rss  exit\n" );  }


/*  end  main  ------------------------------------------------  end  main  */
