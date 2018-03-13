
#include "lib/ParseArg.h"
#include "IPCServer.h"
#include "IPCContext.h"

using namespace singaistorageipc;

int main(int argc,char** argv){
	Parser p(argc,argv);
	IPCContext c("/tmp/ipc_server_socket",10);
	IPCServer s(c);
	s.start();

	return 0;
}