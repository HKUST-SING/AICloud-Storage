/*
 * Define the task structure used in worker interface.
 */

#pragma once


namespace singaistorageipc{

class Task{

public:
	enum class OpType : uint8_t{
		READ = 1,
		WRITE = 2,
		DELETE = 3
	};

	// result infomation
	enum class OpCode : uint8_t{

	};

	/**
	 * Used for create a request.
	 */
	Task(const std::string& username, const std::string& path, 
		OpType opType, uint64_t dataAddr, uint32_t dataSize,
		uint32_t tranID, uint32_t workerID = 0)
		:username_(username),path_(path),opType_(opType),
		dataAddr_(dataAddr),dataSize_(dataSize),tranID_(tranID),
		workerID_(workerID){};

	/**
	 * Used for create a response.
	 */
	Task(const std::string& username, const std::string& path,
		uint32_t tranID, uint32_t workerID, OpCode opCode)
		:username_(username),path_(path),tranID_(tranID),
		workerID_(workerID),opCode_(opCode){};


	std::string getUsername(){
		return username_;
	}

	std::string getPath(){
		return path_;
	}

	OpCode getOpType(){
		return opType_;
	}

	uint64_t getDataAddress(){
		return dataAddr_;
	}

	uint32_t getDataSize(){
		return dataSize_;
	}

	uint32_t getTransctionID(){
		return tranID_;
	}

	uint32_t getWorkerID(){
		return workerID_;
	}

	OpCode getOpCode(){
		return opCode_;
	}

private:
	std::string username_;
	std::string path_;
	OpType opType_;
	uint64_t dataAddr_;
	uint32_t dataSize_;
	uint32_t tranID_;
	uint32_t workerID_;
	OpCode opCode_;

};


}