#ifndef __KERNEL_PROC_H
#define __KERNEL_PROC_H

/**
  @file kernel_proc.h
  @brief The process table and process management.

  @defgroup proc Processes
  @ingroup kernel
  @brief The process table and process management.

  This file defines the PCB structure and basic helpers for
  process access.

  @{
*/ 

#include "tinyos.h"
#include "kernel_sched.h"

/**
  @brief PID state

  A PID can be either free (no process is using it), ALIVE (some running process is
  using it), or ZOMBIE (a zombie process is using it).
  */
typedef enum pid_state_e {
  FREE,   /**< @brief The PID is free and available */
  ALIVE,  /**< @brief The PID is given to a process */
  ZOMBIE  /**< @brief The PID is held by a zombie */
} pid_state;

/**
  @brief Process Control Block.

  This structure holds all information pertaining to a process.
 */
typedef struct process_control_block {
  pid_state  pstate;      /**< @brief The pid state for this PCB */

  PCB* parent;            /**< @brief Parent's pcb. */
  int exitval;            /**< @brief The exit value of the process */

  TCB* main_thread;       /**< @brief The main thread */
  Task main_task;         /**< @brief The main thread's function */
  int argl;               /**< @brief The main thread's argument length */
  void* args;             /**< @brief The main thread's argument string */

  rlnode children_list;   /**< @brief List of children */
  rlnode exited_list;     /**< @brief List of exited children */

  rlnode children_node;   /**< @brief Intrusive node for @c children_list */
  rlnode exited_node;     /**< @brief Intrusive node for @c exited_list */

  CondVar child_exit;     /**< @brief Condition variable for @c WaitChild. 

                             This condition variable is  broadcast each time a child
                             process terminates. It is used in the implementation of
                             @c WaitChild() */

  FCB* FIDT[MAX_FILEID];  /**< @brief The fileid table of the process */

  rlnode ptcb_list;       /*Head of the PTCB list*/
  int thread_count;       /*Number of total threads that run under this PCB*/

} PCB;

/**
  @brief Process Thread Control Block.

  This structure holds all information pertaining to a process thread.
 */
typedef struct process_thread_control_block{
  PCB* owner;   /*Points to the parent process*/
  TCB* tcb;     /*Points to the assigned thread of the TCB*/
  Task task;    /*Task that tcb will run*/
  int argl;     /*argl of Task*/
  void* args;   /*args of Task*/

  int exitval;  /*exitval of thread*/

  int exited;   /*exited status of thread. =1 if thread is exited*/
  int detached; /*detached status of thread. =1 if thread is detached*/
  CondVar exit_cv;  /*Condition variable used to keep waiting threads*/

  int refcount;           /*Number of threads waiting to join*/
  rlnode ptcb_list_node;  /*PTCB list node*/

} PTCB;

typedef struct process_info_control_block{
  procinfo pinfo;
  int PCBcursor;
}procinfo_cb;

/**
  @brief Initialize the process control table.

  This function is called after the spawn of a thread, to initialize
  any data structures related to thread creation.
*/
void initialize_PTCB(TCB* tcb);

/**
  @brief Initialize the process table.

  This function is called during kernel initialization, to initialize
  any data structures related to process creation.
*/
void initialize_processes();

/**
  @brief Get the PCB for a PID.

  This function will return a pointer to the PCB of 
  the process with a given PID. If the PID does not
  correspond to a process, the function returns @c NULL.

  @param pid the pid of the process 
  @returns A pointer to the PCB of the process, or NULL.
*/
PCB* get_pcb(Pid_t pid);

/**
  @brief Get the PID of a PCB.

  This function will return the PID of the process 
  whose PCB is pointed at by @c pcb. If the pcb does not
  correspond to a process, the function returns @c NOPROC.

  @param pcb the pcb of the process 
  @returns the PID of the process, or NOPROC.
*/
Pid_t get_pid(PCB* pcb);

/**
  @brief Start next thread.

  This function is similar to start_main_thread and is used to
  start the next thread (not the first one of the PCB). Provided
  as an argument inside ThreadCreate.
*/
void start_next_thread();
/** @} */

int procinfo_read(void* procinfo_t, char* buf, unsigned int size);

int procinfo_close(void* procinfo_t);

#endif
