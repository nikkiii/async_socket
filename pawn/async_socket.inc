/*
**
*/
#if defined _async_socket_included
 #endinput
#endif
#define _async_socket_included

typedef AsyncSocketConnectCallback = function void(AsyncSocket socket);

typedef AsyncSocketErrorCallback = function void(AsyncSocket socket, int error, const char[] errorName);

typedef AsyncSocketDataCallback = function void(AsyncSocket socket, const char[] data, const int size);

methodmap AsyncSocket < Handle {
	public native AsyncSocket();
	
	public native bool Connect(const char[] host, const int port);
	
	public native bool Write(const char[] data);
	
	public native bool SetConnectCallback(AsyncSocketConnectCallback callback);
	
	public native bool SetErrorCallback(AsyncSocketErrorCallback callback);
	
	public native bool SetDataCallback(AsyncSocketDataCallback callback);
}

/*
#if !defined REQUIRE_EXTENSIONS
public __ext_INTERFACE_SetNTVOptional()
{
	MarkNativeAsOptional("");
}
#endif
*/

public Extension __ext_INTERFACE = 
{
	name = "async_socket",
	file = "async_socket.ext",
#if defined AUTOLOAD_EXTENSIONS
	autoload = 1,
#else
	autoload = 0,
#endif
#if defined REQUIRE_EXTENSIONS
	required = 1,
#else
	required = 0,
#endif
};
