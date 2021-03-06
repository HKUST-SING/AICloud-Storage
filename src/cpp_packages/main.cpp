
#include "lib/utils/ParseArg.h"
#include "IPCServer.h"
#include "IPCContext.h"

using namespace singaistorageipc;

int main(int argc,char** argv){
	Parser p(argc,argv);
	IPCContext c("/tmp/sing_ipc_socket",10);
	IPCServer s(c);
	s.start();
	s.stop();

	return 0;
}
