#pragma once

#include "ipc/lib/message/IPCWriteRequestMessage.h"
#include "ipc/lib/message/IPCReadRequestMessage.h"

namespace singaistorageipc{

/**
 * This class is contain the infomation
 * of current workerID, tranID, remain size,
 * and pending requests of a processing object.
 */
class RequestContext{

public:
    uint32_t workerID_;
};

class ReadRequestContext : public RequestContext{

public:
	uint64_t remainSize_;

	IPCWriteRequestMessage lastResponse_;

	std::queue<IPCReadRequestMessage> pendingList_;
};

class WriteRequestContext :  public RequestContext{

public:
	std::set<uint32_t> processingRequests_;
};

}
