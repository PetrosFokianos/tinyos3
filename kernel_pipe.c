#include "kernel_proc.h"
#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_pipe.h"
#include "kernel_cc.h"


static file_ops reader_file_ops = {
  
  .Open = NULL,
  .Read = pipe_read,
  .Write = NULL,
  .Close = pipe_reader_close
};

static file_ops writer_file_ops = {
  .Open = NULL,
  .Read = NULL,
  .Write = pipe_write,
  .Close = pipe_writer_close
};

int sys_Pipe(pipe_t* pipe)
{
	FCB* fd[2];
  
	Fid_t fid[2];

	if(!FCB_reserve(2,fid,fd)) 
    return -1;
  
	pipe_cb* pipe_control_block = xmalloc(sizeof(pipe_cb));
	fd[0]->streamobj = pipe_control_block;
	fd[1]->streamobj = pipe_control_block;
	pipe_control_block->reader = fd[0];
	pipe_control_block->writer = fd[1];
	pipe_control_block->r_position = 0;
	pipe_control_block->w_position = 0;
  pipe_control_block->data_size = 0;
	pipe_control_block->has_data = COND_INIT;
	pipe_control_block->has_space = COND_INIT;

  fd[0]->streamfunc = &reader_file_ops;
	fd[1]->streamfunc = &writer_file_ops;

	pipe->read = fid[0];
	pipe->write = fid[1];

	return 0;
}

            

int pipe_write(void* pipecb_t, const char *buf, unsigned int n)
{
	pipe_cb* p = (pipe_cb*)pipecb_t; //take curr pipe

	if(p->reader==NULL || p->writer==NULL)
		return -1;
	
	for(int i=0 ; i<n ; i++){
		while((p->w_position+1)%PIPE_BUFFER_SIZE == p->r_position)
			kernel_wait(&p->has_space, SCHED_PIPE);
		p->BUFFER[(p->w_position)%PIPE_BUFFER_SIZE] = buf[i];
		p->w_position = (p->w_position+1)%PIPE_BUFFER_SIZE;
    p->data_size++;
		kernel_broadcast(&p->has_data);
		if(p->reader==NULL)
			return -1;
		
	}

	
	/** Write operation.

    Write up to 'size' bytes from 'buf' to the stream 'this'.
    If it is not possible to write any data (e.g., a buffer is full),
    the thread will block. 
    The write function should return the number of bytes copied from buf, 
    or -1 on error. 

    Possible errors are:
    - There was a I/O runtime problem.
  */
  return n;
}

int pipe_read(void* pipecb_t, char *buf, unsigned int n)
{
	pipe_cb* p = (pipe_cb*)pipecb_t; //take curr pipe

	if(p->reader==NULL)
		return -1;

  if(p->writer == NULL && p->data_size == 0)
    return 0;
	
	for(int i=0 ; i<n ; i++){
		while((p->r_position)%PIPE_BUFFER_SIZE == p->w_position && p->writer!=NULL)
			kernel_wait(&p->has_data, SCHED_PIPE);
		buf[i] = p->BUFFER[(p->r_position)%PIPE_BUFFER_SIZE];
		p->r_position = (p->r_position+1)%PIPE_BUFFER_SIZE;
    p->data_size--;
		kernel_broadcast(&p->has_space);
		if(p->writer==NULL && p->data_size<=0){
			return i+1;
		}
	}
	/** Read operation.

    Read up to 'size' bytes from stream 'this' into buffer 'buf'. 
    If no data is available, the thread will block, to wait for data.
    The Read function should return the number of bytes copied into buf, 
    or -1 on error. The call may return fewer bytes than 'size', 
    but at least 1. A value of 0 indicates "end of data".

    Possible errors are:
    - There was a I/O runtime problem.
  */
 return n;
}

int pipe_writer_close(void* _pipecb)
{

  pipe_cb* p = (pipe_cb*)_pipecb; //take curr pipe
  if(p==NULL)
  	return -1;

  p->writer=NULL; //close writer
  kernel_broadcast(&p->has_data);
  if(p->reader==NULL) //if no reader, free the allocated memory
  	free(p);

  return 0;
	/** Close operation.

      Close the stream object, deallocating any resources held by it.
      This function returns 0 is it was successful and -1 if not.
      Although the value in case of failure is passed to the calling process,
      the stream should still be destroyed.

    Possible errors are:
    - There was a I/O runtime problem.
     */
}

int pipe_reader_close(void* _pipecb)
{

  pipe_cb* p = (pipe_cb*)_pipecb; //take curr pipe
  if(p==NULL)
  return -1;

  p->reader=NULL; //close reader
  kernel_broadcast(&p->has_space);
  if(p->writer==NULL) //if no writer, free the allocated memory
  	free(p);

  return 0;
	/** Close operation.

      Close the stream object, deallocating any resources held by it.
      This function returns 0 is it was successful and -1 if not.
      Although the value in case of failure is passed to the calling process,
      the stream should still be destroyed.

    Possible errors are:
    - There was a I/O runtime problem.
     */
}