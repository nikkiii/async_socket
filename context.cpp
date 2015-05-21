#include "context.h"

AsyncSocketContext::AsyncSocketContext(IPluginContext* pContext) {
	this->pContext = pContext;
	
	connectCallback = forwards->CreateForwardEx(NULL, ET_Single, 1, NULL, Param_Cell);
	errorCallback = forwards->CreateForwardEx(NULL, ET_Single, 3, NULL, Param_Cell, Param_Cell, Param_String);
	dataCallback = forwards->CreateForwardEx(NULL, ET_Single, 3, NULL, Param_Cell, Param_String, Param_Cell);
}

AsyncSocketContext::~AsyncSocketContext() {
	if (connect_req != NULL) {
		free(connect_req);
	}

	if (socket != NULL) {
		uv_close((uv_handle_t *) socket, NULL);
	}

	if (connectCallback) {
		forwards->ReleaseForward(connectCallback);
	}

	if (errorCallback) {
		forwards->ReleaseForward(errorCallback);
	}

	if (dataCallback) {
		forwards->ReleaseForward(dataCallback);
	}
}

void AsyncSocketContext::Connected() {
	if (!connectCallback) {
		return;
	}

	connectCallback->PushCell(hndl);
    connectCallback->Execute(NULL);
}

void AsyncSocketContext::OnError(int error) {
	if (!errorCallback) {
		return;
	}

	errorCallback->PushCell(hndl);
	errorCallback->PushCell(error);
	errorCallback->PushString(uv_err_name(error));
	errorCallback->Execute(NULL);
}

void AsyncSocketContext::OnData(char* data, ssize_t size) {
	if (!dataCallback) {
		return;
	}

	dataCallback->PushCell(hndl);
	dataCallback->PushString(data);
	dataCallback->PushCell(size);
    dataCallback->Execute(NULL);
}

bool AsyncSocketContext::SetConnectCallback(funcid_t function) {
	return connectCallback->AddFunction(pContext, function);
}

bool AsyncSocketContext::SetErrorCallback(funcid_t function) {
	return errorCallback->AddFunction(pContext, function);
}

bool AsyncSocketContext::SetDataCallback(funcid_t function) {
	return dataCallback->AddFunction(pContext, function);
}