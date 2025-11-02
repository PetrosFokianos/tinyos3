
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"

/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
  PTCB* newPTCB = malloc(sizeof(*newPTCB));
  TCB* newTCB = spawn_thread(CURPROC, start_next_thread);
  initialize_PTCB(newTCB);
  newTCB->owner_ptcb->task = task;
  newTCB->owner_ptcb->argl = argl;
  newTCB->owner_ptcb->args = args;
  wakeup(newTCB);
  return (Tid_t) newTCB;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) cur_thread();
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
	return -1;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
	return -1;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{
  
}

