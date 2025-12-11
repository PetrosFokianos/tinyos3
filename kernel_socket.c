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

FCB* PORT_MAP[MAX_PORT+1];

Fid_t sys_Socket(port_t port)
{
	socket_cb* socket = (socket_cb*)malloc(sizeof(socket_cb));
	FCB* fd;
	Fid_t fid;

	if(port==0)
		return NOPORT;
	 
	if(port<0 || port > MAX_PORT)
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
	
	switch (sock->type)
	{
	case SOCKET_PEER:
		pipe_read(sock->peer_s.read_pipe,buf,n);
		return 0;
	
	default:
		return -1;
	}
}

int socket_write(void* socket_t, const char *buf, unsigned int n){
	socket_cb* sock = (socket_cb*)socket_t;
	
	switch (sock->type)
	{
	case SOCKET_PEER:
		pipe_write(sock->peer_s.write_pipe,buf,n);
		return 0;
	default:
		return -1;
	}
}

int socket_close(void* socket_t){
	socket_cb* sock = (socket_cb*)socket_t;
	sock->fcb = NULL;
	if(sock->type == SOCKET_LISTENER)
		PORT_MAP[sock->port] = NULL;
	else if(sock->type == SOCKET_PEER){
		sock->peer_s.peer = NULL;
		pipe_reader_close(sock->peer_s.read_pipe);
		pipe_writer_close(sock->peer_s.write_pipe);
	}
	sock = NULL;
	return 0;
}

int sys_Listen(Fid_t sock)
{
	if(sock<0 || sock>MAX_FILEID)
		return -1;

	FCB* fd = get_fcb(sock);
	
	if(fd == NULL)
		return -1;

	socket_cb* socket = fd->streamobj;


	if(socket==NULL)
		return -1;

	if(socket->port == 0 || socket->type!=SOCKET_UNBOUND)
		return -1;

	if(PORT_MAP[socket->port]!=NULL)
		return -1;
	
	socket->type = SOCKET_LISTENER;
	PORT_MAP[socket->port] = socket->fcb;
	rlnode_init(&socket->listener_s.queue, NULL);
	socket->listener_s.req_available = COND_INIT;
	return 0;
}


Fid_t sys_Accept(Fid_t lsock)
{
	if(lsock<0 || lsock>MAX_FILEID)
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
	while(is_rlist_empty(&socket->listener_s.queue))
		kernel_wait(&socket->listener_s.req_available, SCHED_IO);
	Req* req= rlist_pop_front(&socket->listener_s.queue)->req;
	peer = req->peer;

	Fid_t newFID = sys_Socket(socket->port);
	FCB* newfd = get_fcb(newFID);
	socket_cb* newpeer = newfd->streamobj;

	newpeer->type = SOCKET_PEER;
	newpeer->peer_s.peer = peer;
	peer->peer_s.peer = newpeer;
	newpeer->peer_s.read_pipe = peer->peer_s.write_pipe;
	newpeer->peer_s.write_pipe = peer->peer_s.read_pipe;
	req->admitted = 1;
	kernel_broadcast(&req->connected_cv);
	socket -> refcount --;
	
	return newFID;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	if(sock<0 || sock>MAX_FILEID-1)
		return -1;

	if(port==0)
		return -1;
	 
	if(port<0 || port > MAX_PORT)
	    return -1;
	
	FCB* fd = get_fcb(sock);

	if(fd == NULL)
		return -1;

	socket_cb* socket = fd->streamobj;

	if(socket == NULL)
		return -1;

	if (socket->type != SOCKET_UNBOUND)
        return -1;
	//Do all the checks needed for socket

	if(PORT_MAP[port] == NULL)
		return -1;

	socket_cb* listener = PORT_MAP[port]->streamobj;
	
    if (listener == NULL || listener->type != SOCKET_LISTENER){
        return -1;
	}

	//Do all the checks needed for the listener at that port

	socket->refcount++; //Increase the socket's refcount

	Req *request = (Req *)malloc(sizeof(request));

	//Build the request
	request->admitted = 0;
    request->peer = socket;
    request->connected_cv = COND_INIT;
    rlnode_init(&request->queue_node, request);

	//Add request to the listener’s request queue and signal listener
	rlist_push_back(&listener->listener_s.queue, &request->queue_node);

	kernel_broadcast(&listener->listener_s.req_available);

	//While request is not admitted, the connect call will block for the specified amount of time
	if(request->admitted==0)
		kernel_timedwait(&request->connected_cv, SCHED_IO, timeout);
	
	//At some point, Accept() will serve the request (req->admitted = 1) and signal the Connect side

	socket->refcount--; //Decrease refcount
	rlist_remove(&request->queue_node);

	if(!request->admitted)
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
