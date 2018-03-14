//#include "lib/IPCServer.h"
//#include "lib/IPCSocketPoolContext.h"
//#include <folly/io/async/AsyncSocket.h>
//#include <folly/io/async/EventBase.h>

#include <iostream>
#include <cstring>
#include <unordered_map>
#include <queue>
#include <cstdlib>
#include <memory>

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

class Example{

public:
	Example(const Example& e)
	: queue(e.queue){std::srand(time(NULL));};

	Example(){};

	std::queue<int> queue;

};


int main(int argc, char** argv){
	Example tmp;
	tmp.queue.emplace(1); 
	std::cout << tmp.queue.size() << std::endl;

	Example tmp1(tmp);
	tmp1.queue.emplace(2);
	std::cout << tmp.queue.size() << std::endl;
	std::srand(time(NULL));
	std::cout << std::rand() << std::endl;
	auto a = std::make_shared<Example>(tmp);
	std::cout << a->queue.size() << std::endl;
	a->queue.emplace(1);
	std::cout << a->queue.size() << std::endl;
	return 0;
}
