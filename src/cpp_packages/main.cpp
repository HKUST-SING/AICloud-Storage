#pragma once

#include "lib/ParseArg.h"
#include "lib/IPCServer.h"

using namespace singaistorageipc;

int main(int argc,char** argv){
	Parser p(argc,argv);
	IPCServer s;
	s.start();

	return 0;
}