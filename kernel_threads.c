
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"

/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
  TCB* newTCB = spawn_thread(CURPROC, start_next_thread);
  initialize_PTCB(newTCB);
  newTCB->owner_ptcb->task = task;
  newTCB->owner_ptcb->argl = argl;
  newTCB->owner_ptcb->args = args;
  wakeup(newTCB);
  return (Tid_t) newTCB->owner_ptcb;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) cur_thread()->owner_ptcb;
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
  PTCB* ptcb;
  ptcb = (PTCB *) tid;
  if(tid == NOTHREAD || rlist_find(&CURPROC->ptcb_list, ptcb, NULL)==NULL){
	  return -1;
  }
  else if (ptcb->exited==1 || ptcb->detached==1 || ptcb == cur_thread()->owner_ptcb){
    return -1;
  }
  else{
    while(ptcb->exited==0)
      kernel_wait(&cur_thread()->owner_ptcb->exit_cv,SCHED_IDLE);
  }
  return 0;
 
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
  PTCB* ptcb;
  ptcb = (PTCB *) tid;


  if(tid == NOTHREAD || rlist_find(&CURPROC->ptcb_list, ptcb, NULL)==NULL){
	  return -1;
  }
  if(ptcb->exited==0)
    ptcb->detached = 1;
  return 0;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{

}

