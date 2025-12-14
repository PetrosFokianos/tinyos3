#include "kernel_dev.h"
#include "tinyos.h"
#include "kernel_pipe.h"
#include "kernel_socket.h"
#include "kernel_cc.h"
#include "util.h"

static file_ops socket_file_ops = {
  .Open = NULL,
  .Read = socket_read,
  .Write = socket_write,
  .Close = socket_close
};

socket_cb* PORT_MAP[MAX_PORT+1];

Fid_t sys_Socket(port_t port)
{
	socket_cb* socket = xmalloc(sizeof(socket_cb));
	FCB* fd;
	Fid_t fid;

	 
	if(port<NOPORT || port > MAX_PORT)
	    return NOFILE;
	
	if(!FCB_reserve(1,&fid,&fd)) 
    	return NOFILE;

	fd->streamfunc = &socket_file_ops;
	fd->streamobj = socket;
	socket->type = SOCKET_UNBOUND;
	socket->refcount = 0;
	socket->port=port;
	socket->fcb=fd;
	

	return fid;
}

int socket_read(void* socket_t, char *buf, unsigned int n){
	socket_cb* sock = (socket_cb*)socket_t;
	
	if(sock==NULL)
		return -1;

	if(sock->type!=SOCKET_PEER)
		return -1;

	if(sock->peer_s.read_pipe == NULL)
		return -1;

	return pipe_read(sock->peer_s.read_pipe, buf, n);
}

int socket_write(void* socket_t, const char *buf, unsigned int n){
	socket_cb* sock = (socket_cb*)socket_t;

	if(sock==NULL)
		return -1;

	if(sock->type!=SOCKET_PEER)
		return -1;

	if(sock->peer_s.write_pipe == NULL)
		return -1;
	return pipe_write(sock->peer_s.write_pipe, buf, n);
}

int socket_close(void* socket_t){
	socket_cb* sock = (socket_cb*)socket_t;

	if(sock==NULL)
		return -1;
	
	
	if(sock->type == SOCKET_LISTENER){
		PORT_MAP[sock->port] = NULL;
	    kernel_broadcast(&sock->listener_s.req_available);
	}
	else if(sock->type == SOCKET_PEER){
		sock->peer_s.peer = NULL;
		pipe_reader_close(sock->peer_s.read_pipe);
		pipe_writer_close(sock->peer_s.write_pipe);
	}
	
	if(sock->refcount == 0)
		free(sock);

	return 0;
}

int sys_Listen(Fid_t sock)
{
	if(sock<0 || sock>=MAX_FILEID)
		return -1;
	FCB* fd = get_fcb(sock);
	

	if(fd == NULL)
		return -1;

	socket_cb* socket = fd->streamobj;


	if(socket==NULL)
		return -1;

	if(socket->port <= NOPORT  || socket->port > MAX_PORT )
	  return -1;

	if(socket->type!=SOCKET_UNBOUND)
		return -1;

	if(PORT_MAP[socket->port]!=NULL)
		return -1;
	
	socket->type = SOCKET_LISTENER;
	PORT_MAP[socket->port] = socket;
	rlnode_init(&socket->listener_s.queue, NULL);
	socket->listener_s.req_available = COND_INIT;
	return 0;
}


Fid_t sys_Accept(Fid_t lsock)
{
	if(lsock<0 || lsock>=MAX_FILEID)
		return NOFILE;

	FCB* fd = get_fcb(lsock);

	if(fd == NULL)
		return NOFILE;

	socket_cb* socket = fd->streamobj;
	socket_cb* peer;

	if(socket==NULL)
		return NOFILE;
	
	if(socket->type != SOCKET_LISTENER)
		return NOFILE;

	socket ->refcount ++;
	while(is_rlist_empty(&socket->listener_s.queue) && PORT_MAP[socket->port]!=NULL)
		kernel_wait(&socket->listener_s.req_available, SCHED_IO);
	
	if (PORT_MAP[socket->port] == NULL){
		socket->refcount--;
		return -1;
	}
	
	
	Req* req= rlist_pop_front(&socket->listener_s.queue)->req;
	peer = req->peer;
	peer->type = SOCKET_PEER;

	Fid_t newFID = sys_Socket(socket->port);
	FCB* newfd = get_fcb(newFID);
	if(newfd == NULL){
		kernel_signal(&req->connected_cv);
		socket->refcount--;
		return NOFILE;
	}
	socket_cb* newpeer = newfd->streamobj;


	FCB* fcb = xmalloc(sizeof(FCB));
	pipe_cb* pipe1 = xmalloc(sizeof(pipe_cb));
	pipe_cb* pipe2 = xmalloc(sizeof(pipe_cb));


	peer->peer_s.read_pipe = pipe1;
	peer->peer_s.write_pipe = pipe2;
	newpeer->peer_s.read_pipe = pipe2;
	newpeer->peer_s.write_pipe = pipe1;


	pipe1->reader = fcb;
	pipe1->writer = fcb;
	pipe1->r_position = 0;
	pipe1->w_position = 0;
  	pipe1->data_size = 0;
	pipe1->has_data = COND_INIT;
	pipe1->has_space = COND_INIT;


	pipe2->reader = fcb;
	pipe2->writer = fcb;
	pipe2->r_position = 0;
	pipe2->w_position = 0;
  	pipe2->data_size = 0;
	pipe2->has_data = COND_INIT;
	pipe2->has_space = COND_INIT;

	newpeer->peer_s.read_pipe = pipe2;
	newpeer->peer_s.write_pipe = pipe1;
	peer->peer_s.read_pipe = pipe1;
	peer->peer_s.write_pipe = pipe2;

	newpeer->type = SOCKET_PEER;
	newpeer->peer_s.peer = peer;
	peer->peer_s.peer = newpeer;
	req->admitted = 1;
	kernel_signal(&req->connected_cv);
	socket->refcount --;
	
	return newFID;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	if(sock<0 || sock>MAX_FILEID-1)
		return -1;


	if(port<NOPORT || port > MAX_PORT)
	    return -1;

	FCB* fd = get_fcb(sock);

	if(fd == NULL)
		return -1;

	if(fd->streamobj == NULL)
		return -1;

	 
	socket_cb* socket = fd->streamobj;


	if (socket->type != SOCKET_UNBOUND)
        return -1;
	//Do all the checks needed for socket

	if(PORT_MAP[port] == NULL)
		return -1;

	socket_cb* listener = PORT_MAP[port];
	
    if (listener == NULL || listener->type != SOCKET_LISTENER){
        return -1;
	}

	//Do all the checks needed for the listener at that port

	socket->refcount++; //Increase the socket's refcount

	Req *request = xmalloc(sizeof(Req));

	//Build the request
	request->admitted = 0;
    request->peer = socket;
    request->connected_cv = COND_INIT;
    rlnode_init(&request->queue_node, request);

	//Add request to the listener’s request queue and signal listener
	rlist_push_back(&listener->listener_s.queue, &request->queue_node);

	kernel_signal(&listener->listener_s.req_available);

	//While request is not admitted, the connect call will block for the specified amount of time
	while(request->admitted==0){
		if(kernel_timedwait(&request->connected_cv, SCHED_IO, timeout)==0)
			break;
	}
		
	
	//At some point, Accept() will serve the request (req->admitted = 1) and signal the Connect side

	socket->refcount--; //Decrease refcount

	if(request->admitted==0)
		return -1;

	return 0;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	if(sock < 0 || sock >= MAX_FILEID)
		return -1;

	FCB* fd = get_fcb(sock);

	if(fd == NULL)
		return -1;

	socket_cb* socket = fd->streamobj;

	if(socket == NULL)
		return -1;

    if(socket->type == SOCKET_PEER){
		switch (how)
		{
		case SHUTDOWN_READ:
			pipe_reader_close(socket->peer_s.read_pipe);
			socket->peer_s.read_pipe = NULL;
			break;
		case SHUTDOWN_WRITE:
			pipe_writer_close(socket->peer_s.write_pipe);
			socket->peer_s.write_pipe = NULL;
			break;
		case SHUTDOWN_BOTH:
				pipe_reader_close(socket->peer_s.read_pipe);
				pipe_writer_close(socket->peer_s.write_pipe);
				socket->peer_s.read_pipe = NULL;
				socket->peer_s.write_pipe = NULL;
				break;

		default:
			return -1;
		}
		
		return 0;
	}
	else
		return -1;
}
