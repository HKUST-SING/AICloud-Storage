//#include "lib/IPCServer.h"
//#include "lib/IPCSocketPoolContext.h"
//#include <folly/io/async/AsyncSocket.h>
//#include <folly/io/async/EventBase.h>

#include <iostream>
#include <cstring>
#include <unordered_map>
#include <queue>

#include <stdio.h>
#include <ctime>
/*
#include "../lib/message/IPCAuthenticationMessage.h"
#include "../lib/message/IPCConnectionReplyMessage.h"
#include "../lib/message/IPCReadRequestMessage.h"
#include "../lib/message/IPCWriteRequestMessage.h"
#include "../lib/message/IPCStatusMessage.h"
#include "../lib/message/IPCCloseMessage.h"
*/
//using namespace singaistorageipc;


int main(int argc, char** argv){
	uint32_t readBufferSize_ = 1000*1000*1000;
	std::cout << readBufferSize_ << std::endl;

	int a = 2;
	std::cout << (a << 1) << std::endl;
	return 0;
}
