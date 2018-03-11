#pragma once

#include "lib/ParseArg.h"
#include "lib/IPCServer.h"
#include "lib/IPCContext.h"
#include "lib/message/IPCAuthentication.h"

using namespace singaistorageipc;

int main(int argc,char** argv){
	Parser p(argc,argv);
	IPCContext c("/tmp/ipc_server_socket",10);
	IPCServer s(c);
	IPCAuthentication msg;
	s.start();

	return 0;
}