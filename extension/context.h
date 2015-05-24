#ifndef ASYNC_SOCKET_CONTEXT_H
#define ASYNC_SOCKET_CONTEXT_H

#include <stdlib.h>
#include <uv.h>

#include "smsdk_ext.h"

class AsyncSocketContext {
public:
    IPluginContext* pContext;

	Handle_t hndl;

	char* host;
	int port;

    IChangeableForward *connectCallback;
	IChangeableForward *errorCallback;
	IChangeableForward *dataCallback;

	uv_getaddrinfo_t resolver;
	uv_connect_t* connect_req;
	uv_tcp_t* socket;
	uv_stream_t* stream;

    AsyncSocketContext(IPluginContext* plugin);
    ~AsyncSocketContext();
	
	void Connected();

	void OnError(int error);

	void OnData(char* data, ssize_t size);

	bool SetConnectCallback(funcid_t function);
	bool SetErrorCallback(funcid_t function);
	bool SetDataCallback(funcid_t function);
};

#endif