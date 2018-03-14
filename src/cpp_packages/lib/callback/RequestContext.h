#pragma once

#include "../message/IPCWriteRequestMessage.h"
#include "../message/IPCReadRequestMessage.h"

namespace singaistorageipc{

/**
 * This class is contain the infomation
 * of current workerID, tranID, remain size,
 * and pending requests of a processing object.
 */
class RequestContext{

public:
    uint32_t workerID_;

    uint32_t tranID_;   

};

class ReadRequestContext : public RequestContext{

public:
	uint32_t remainSize_;

	IPCWriteRequestMessage lastResponse_;

	std::queue<IPCReadRequestMessage> pendingList_;
};

class WriteRequestContext :  public RequestContext{

public:
	IPCReadRequestMessage lastResponse_;

	std::queue<IPCWriteRequestMessage> pendingList_;

};

}