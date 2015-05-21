/**
 * vim: set ts=4 :
 * =============================================================================
 * SourceMod Sample Extension
 * Copyright (C) 2004-2008 AlliedModders LLC.  All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */

#include "extension.h"
#include "queue.h"
#include "context.h"
#include <uv.h>

/**
 * @file extension.cpp
 * @brief Implement extension code here.
 */

LockedQueue<AsyncSocketContext*> g_connect_queue;
LockedQueue<socket_data_t*> g_data_queue;
LockedQueue<error_data_t*> g_error_queue;

uv_loop_t *loop;

uv_thread_t loop_thread;

uv_async_t g_async_resolve;
uv_async_t g_async_write;

AsyncSocket g_AsyncSocket;		/**< Global singleton for extension's main interface */

SMEXT_LINK(&g_AsyncSocket);

void push_error(AsyncSocketContext *ctx, int error);

AsyncSocketContext* AsyncSocket::GetSocketInstanceByHandle(Handle_t handle) {
	HandleSecurity sec;
	sec.pOwner = NULL;
	sec.pIdentity = myself->GetIdentity();
	
	AsyncSocketContext *client;

	if (handlesys->ReadHandle(handle, socketHandleType, &sec, (void**)&client) != HandleError_None)
		return NULL;

	return client;
}

void AsyncSocket::OnHandleDestroy(HandleType_t type, void *object) {
	if(object != NULL) {
		AsyncSocketContext *ctx = (AsyncSocketContext *) object;

		if (ctx->connect_req != NULL) {
			uv_close((uv_handle_t *) ctx->connect_req->handle, NULL);
			free(ctx->connect_req);
		}

		if (ctx->socket != NULL) {
			free(ctx->socket);
		}

		delete ctx;
	}
}

void OnGameFrame(bool simulating) {
	if (!g_connect_queue.Empty()) {
		g_connect_queue.Lock();
		while(!g_connect_queue.Empty()) {
			g_connect_queue.Pop()->Connected();
		}
		g_connect_queue.Unlock();
	}

	if (!g_error_queue.Empty()) {
		g_error_queue.Lock();
		while(!g_error_queue.Empty()) {
			error_data_t *err = g_error_queue.Pop();

			err->ctx->OnError(err->err);

			free(err);
		}
		g_error_queue.Unlock();
	}

	if (!g_data_queue.Empty()) {
		g_data_queue.Lock();
		while(!g_data_queue.Empty()) {
			socket_data_t *data = g_data_queue.Pop();

			data->ctx->OnData(data->buf, data->size);

			free(data->buf);
			free(data);
		}
		g_data_queue.Unlock();
	}
}

// main event loop thread
void EventLoop(void* data) {
	uv_run(loop, UV_RUN_DEFAULT);
}

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
	buf->base = (char*) malloc(suggested_size);
	buf->len = suggested_size;
}

void on_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
	if (nread < 0) {
		if (nread != UV_EOF) {
			push_error((AsyncSocketContext*) client->data, nread);
		}
		//uv_close((uv_handle_t*) client, NULL);
		//free(client);
		return;
	}

	char *data = (char*) malloc(sizeof(char) * (nread+1));
	data[nread] = '\0';
	strncpy(data, buf->base, nread);

	socket_data_t *s = (socket_data_t *) malloc(sizeof(socket_data_t));

	s->ctx = static_cast<AsyncSocketContext*>(client->data);
	s->buf = data;
	s->size = nread;

	g_data_queue.Lock();
	g_data_queue.Push(s);
	g_data_queue.Unlock();

	free(buf->base);
}

void on_connect(uv_connect_t *req, int status) {
	if (status < 0) {
		push_error((AsyncSocketContext*) req->data, status);
		return;
	}

	g_connect_queue.Lock();
	g_connect_queue.Push((AsyncSocketContext*) req->data);
	g_connect_queue.Unlock();

	req->handle->data = req->data;

	uv_read_start((uv_stream_t*) req->handle, alloc_buffer, on_read);
}

void push_error(AsyncSocketContext *ctx, int error) {
	error_data_t *err = (error_data_t*) malloc(sizeof(error_data_t));

	err->ctx = ctx;
	err->err = error;

	g_error_queue.Lock();
	g_error_queue.Push(err);
	g_error_queue.Unlock();
}

void on_resolved(uv_getaddrinfo_t *resolver, int status, struct addrinfo *res) {
	if (status < 0) {
		push_error((AsyncSocketContext *) resolver->data, status);
		return;
	}

	AsyncSocketContext *ctx = static_cast<AsyncSocketContext *>(resolver->data);

	uv_connect_t *connect_req = (uv_connect_t*) malloc(sizeof(uv_connect_t));
	uv_tcp_t *socket = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));

	ctx->connect_req = connect_req;
	ctx->socket = socket;

	connect_req->data = ctx;

	char addr[17] = {'\0'};
	uv_ip4_name((struct sockaddr_in*) res->ai_addr, addr, 16);

	uv_tcp_init(loop, socket);

	uv_tcp_connect(connect_req, socket, (const struct sockaddr*) res->ai_addr, on_connect);

	uv_freeaddrinfo(res);
}

cell_t Socket_Create(IPluginContext *pContext, const cell_t *params) {
	AsyncSocketContext *ctx = new AsyncSocketContext(pContext);

	ctx->hndl = handlesys->CreateHandle(g_AsyncSocket.socketHandleType, ctx, pContext->GetIdentity(), myself->GetIdentity(), NULL);

	return ctx->hndl;
}

cell_t Socket_Connect(IPluginContext *pContext, const cell_t *params) {
	AsyncSocketContext *ctx = g_AsyncSocket.GetSocketInstanceByHandle(params[1]);

	if (ctx == NULL) {
		return pContext->ThrowNativeError("Invalid socket handle");
	}

	if (params[3] < 0 || params[3] > 65535) {
		return pContext->ThrowNativeError("Invalid port specified");
	}
	
	char *address = NULL;
	pContext->LocalToString(params[2], &address);

	ctx->host = address;
	ctx->port = params[3];

	g_async_resolve.data = ctx;
	uv_async_send(&g_async_resolve);

	return 1;
}

cell_t Socket_Write(IPluginContext *pContext, const cell_t *params) {
	AsyncSocketContext *ctx = g_AsyncSocket.GetSocketInstanceByHandle(params[1]);

	if (ctx == NULL) {
		return pContext->ThrowNativeError("Invalid socket handle");
	}

	char *data = NULL;
	pContext->LocalToString(params[2], &data);

	uv_buf_t buffer;

	buffer.base = strdup(data);
	buffer.len = strlen(data);

	socket_write_t *write = (socket_write_t *) malloc(sizeof(socket_write_t));

	write->ctx = ctx;
	write->buf = buffer;

	g_async_write.data = write;
	uv_async_send(&g_async_write);

	return 1;
}

cell_t Socket_SetConnectCallback(IPluginContext *pContext, const cell_t *params) {
	AsyncSocketContext *ctx = g_AsyncSocket.GetSocketInstanceByHandle(params[1]);

	if (ctx == NULL) {
		return pContext->ThrowNativeError("Invalid socket handle");
	}

	if (!ctx->SetConnectCallback(params[2])) {
		return pContext->ThrowNativeError("Invalid callback");
	}

	return true;
}

cell_t Socket_SetErrorCallback(IPluginContext *pContext, const cell_t *params) {
	AsyncSocketContext *ctx = g_AsyncSocket.GetSocketInstanceByHandle(params[1]);

	if (ctx == NULL) {
		return pContext->ThrowNativeError("Invalid socket handle");
	}

	if (!ctx->SetErrorCallback(params[2])) {
		return pContext->ThrowNativeError("Invalid callback");
	}

	return true;
}

cell_t Socket_SetDataCallback(IPluginContext *pContext, const cell_t *params) {
	AsyncSocketContext *ctx = g_AsyncSocket.GetSocketInstanceByHandle(params[1]);

	if (ctx == NULL) {
		return pContext->ThrowNativeError("Invalid socket handle");
	}

	if (!ctx->SetDataCallback(params[2])) {
		return pContext->ThrowNativeError("Invalid callback");
	}

	return true;
}

void async_resolve(uv_async_t *handle) {
	AsyncSocketContext *ctx = static_cast<AsyncSocketContext *>(handle->data);

	ctx->resolver.data = ctx;
	
	char *service = (char *) malloc(sizeof(char) * 6);

	sprintf(service, "%d", ctx->port);

	struct addrinfo hints;
	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = 0;

	int r = uv_getaddrinfo(loop, &ctx->resolver, on_resolved, ctx->host, service, &hints);

	if (r) {
		push_error(ctx, r);
	}
}

void async_write(uv_async_t *handle) {
	socket_write_t *data = (socket_write_t *) handle->data;

	uv_write_t req;
	uv_write(&req, data->ctx->connect_req->handle, &data->buf, 1, NULL);

	free(data->buf.base);
	free(data);
}

// Sourcemod Plugin Events
bool AsyncSocket::SDK_OnLoad(char *error, size_t maxlength, bool late) {
	sharesys->AddNatives(myself, AsyncSocketNatives);
	sharesys->RegisterLibrary(myself, "async_socket");

	socketHandleType = handlesys->CreateType("AsyncSocket", this, 0, NULL, NULL, myself->GetIdentity(), NULL);

	smutils->AddGameFrameHook(OnGameFrame);

	loop = uv_default_loop();

	uv_async_init(loop, &g_async_resolve, async_resolve);
	uv_async_init(loop, &g_async_write, async_write);

	uv_thread_create(&loop_thread, EventLoop, NULL);
	
	return true;
}

void AsyncSocket::SDK_OnUnload() {
	handlesys->RemoveType(socketHandleType, NULL);

	uv_thread_join(&loop_thread);

	uv_loop_close(loop);

	smutils->RemoveGameFrameHook(OnGameFrame);
}

const sp_nativeinfo_t AsyncSocketNatives[] = {
	{"AsyncSocket.AsyncSocket", Socket_Create},
	{"AsyncSocket.Connect", Socket_Connect},
	{"AsyncSocket.Write", Socket_Write},
	{"AsyncSocket.SetConnectCallback", Socket_SetConnectCallback},
	{"AsyncSocket.SetErrorCallback", Socket_SetErrorCallback},
	{"AsyncSocket.SetDataCallback", Socket_SetDataCallback},
	{NULL, NULL}
};