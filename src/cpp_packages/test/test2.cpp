//#include "lib/IPCServer.h"
//#include "lib/IPCSocketPoolContext.h"
//#include <folly/io/async/AsyncSocket.h>
//#include <folly/io/async/EventBase.h>

#include <iostream>
#include <cstring>
#include <unordered_map>
#include <queue>

#include "../lib/message/IPCAuthenticationMessage.h"
#include "../lib/message/IPCConnectionReplyMessage.h"
#include "../lib/message/IPCReadRequestMessage.h"
#include "../lib/message/IPCWriteRequestMessage.h"
#include "../lib/message/IPCStatusMessage.h"

using namespace singaistorageipc;

void insert(
	std::unordered_map<std::string,std::queue<IPCReadRequestMessage>> *map,
	IPCReadRequestMessage msg){
	auto search = map->find(msg.getPath());
	if(search == map->end()){
		std::queue<IPCReadRequestMessage> queue;
		queue.emplace(msg);
		map->emplace(msg.getPath(),queue);
	}
	else{
		search->second.emplace(msg);
	}
}

int main(int argc, char** argv){
	std::unordered_map<std::string,std::queue<IPCReadRequestMessage>> map;

	IPCReadRequestMessage r1,r2,r3;
	r1.setPath("a1");
	r2.setPath("a2");
	r3.setPath("a1");

	insert(&map,r1);
	auto search = map.find(r1.getPath());
	if(search != map.end()){
		std::cout << "insert r1 successfully: " 
			<< search->second.size() <<std::endl;
	}
	else{
		std::cout << "insert r1 failed" << std::endl;
	}

	insert(&map,r2);
	search = map.find(r2.getPath());
	if(search != map.end()){
		std::cout << "insert r2 successfully: " 
			<< search->second.size() <<std::endl;
	}
	else{
		std::cout << "insert r2 failed" << std::endl;
	}

	insert(&map,r3);
	search = map.find(r3.getPath());
	if(search != map.end()){
		std::cout << "insert r3 successfully: " 
			<< search->second.size() <<std::endl;
	}
	else{
		std::cout << "insert r3 failed" << std::endl;
	}

	search = map.find(r3.getPath());
	IPCReadRequestMessage r = search->second.front();
	std::cout << r.getPath() << std::endl;
	std::cout << search->second.size() << std::endl;
	search->second.pop();
	
	search = map.find(r3.getPath());
	std::cout << search->second.size() << std::endl;


	return 0;
}
