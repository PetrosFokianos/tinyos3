#ifndef __KERNEL_SOCKET_H
#define __KERNEL_SOCKET_H

#include "kernel_dev.h"
#include "tinyos.h"
#include "kernel_pipe.h"


typedef enum{
    SOCKET_LISTENER,
	SOCKET_UNBOUND,
	SOCKET_PEER
}socket_type;

typedef struct socket_control_block socket_cb;

typedef struct listener_s{
    rlnode queue;
    CondVar req_available;
}listener_socket;

typedef struct unbound_s{

	rlnode unbound_socket;	

} unbound_socket;

typedef struct peer_s{
    pipe_cb* write_pipe;
    pipe_cb* read_pipe;
    socket_cb* peer; //pointer to connected socket 

}peer_socket;

typedef struct socket_control_block{
    int refcount;
    FCB* fcb;
    socket_type type;
    port_t port;

    union{
		listener_socket listener_s;
		peer_socket peer_s;	
		unbound_socket unbound_s;
	};

}socket_cb;





typedef struct connection_request{
    int admitted;
    socket_cb* peer;
    CondVar connected_cv;
    rlnode queue_node;
}Req;


Fid_t sys_Socket(port_t port);
int sys_Listen(Fid_t sock);
Fid_t sys_Accept(Fid_t lsock);
int sys_Connect(Fid_t sock, port_t port, timeout_t timeout);
int sys_ShutDown(Fid_t sock, shutdown_mode how);
int socket_write(void* this, const char *buf, unsigned int n);
int socket_read(void* this, char *buf, unsigned int n);
int socket_close(void* this);



#endif