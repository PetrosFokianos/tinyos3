#include <assert.h>
#include <sys/mman.h>
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"
#include "kernel_streams.h"

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
  else if (ptcb->detached==1 || ptcb == cur_thread()->owner_ptcb){
    return -1;
  }
  else{
    ptcb->refcount++;
    while(ptcb->exited==0){
      kernel_wait(&ptcb->exit_cv,SCHED_USER);
      if(ptcb->detached==1)
          return -1;
    }
    ptcb->refcount--;
  }
  if(exitval!=NULL){
 *exitval = ptcb->exitval;
  }
  if(ptcb->refcount==0)
  rlist_remove(&ptcb->ptcb_list_node);
 
  return 0;
 
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
  PCB* pcb =CURPROC;
  PTCB* ptcb = (PTCB *) tid;


  if(tid == NOTHREAD || rlist_find(&pcb->ptcb_list, ptcb, NULL)==NULL){
	  return -1;
  }
  
  if(ptcb->exited==0){
    ptcb->detached = 1;
    kernel_broadcast(&ptcb->exit_cv);
    return 0;
  }
  return -1;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{
  PCB* curproc = CURPROC;
  TCB* tcb = cur_thread(); /* obtain cur tcb */
  PTCB* ptcb = tcb->owner_ptcb;  /*obtain ptcb*/

  if(curproc ->thread_count == 1) { /*no ptcbs left in the current process*/
    if(get_pid(curproc)==1) {

    while(sys_WaitChild(NOPROC,NULL)!=NOPROC);

  }else{

    /* Reparent any children of the exiting process to the 
       initial task */
    PCB* initpcb = get_pcb(1);
    while(!is_rlist_empty(& curproc->children_list)) {
      rlnode* child = rlist_pop_front(& curproc->children_list);
      child->pcb->parent = initpcb;
      rlist_push_front(& initpcb->children_list, child);
    }

    /* Add exited children to the initial task's exited list 
       and signal the initial task */
    if(!is_rlist_empty(& curproc->exited_list)) {
      rlist_append(& initpcb->exited_list, &curproc->exited_list);
      kernel_broadcast(& initpcb->child_exit);
    }

    /* Put me into my parent's exited list */
    rlist_push_front(& curproc->parent->exited_list, &curproc->exited_node);
    kernel_broadcast(& curproc->parent->child_exit);
  }

    assert(is_rlist_empty(& curproc->children_list));
  assert(is_rlist_empty(& curproc->exited_list));


  /* 
    Do all the other cleanup we want here, close files etc. 
   */

  /* Release the args data */
  if(curproc->args) {
    free(curproc->args);
    curproc->args = NULL;
  }

  /* Clean up FIDT */
  for(int i=0;i<MAX_FILEID;i++) {
    if(curproc->FIDT[i] != NULL) {
      FCB_decref(curproc->FIDT[i]);
      curproc->FIDT[i] = NULL;
    }
  }

    /* Disconnect my main_thread */
    curproc->main_thread = NULL;

    /* Now, mark the process as exited. */
    curproc->pstate = ZOMBIE;

  }
    ptcb-> exitval= exitval;
    ptcb-> exited=1; /*makes it exited*/
    curproc->thread_count --;

    kernel_broadcast(&ptcb->exit_cv);

  kernel_sleep(EXITED, SCHED_USER);
}