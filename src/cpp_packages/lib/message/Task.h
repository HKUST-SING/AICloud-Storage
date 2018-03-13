/*
 * Define the task structure used in worker interface.
 */

#pragma once

#include <string>
#include <utility>

namespace singaistorageipc{

class Task{

public:
	enum class OpType : uint8_t{
		READ = 1,
		WRITE = 2,
		DELETE = 3,
        CLOSE  = 4
	};

	// result infomation
	enum class OpCode : uint8_t{

	};

	/**
	 * Used for create a request.
	 */
	Task(const std::string& username, const std::string& path, 
        const OpType opType, const uint64_t dataAddr, 
        const uint32_t dataSize, const uint32_t tranID,
		const uint32_t workerID = 0)
		: username_(username),
          path_(path),
          opType_(opType),
		  dataAddr_(dataAddr),
          dataSize_(dataSize),
          tranID_(tranID),
		  workerID_(workerID)

    {}

	/**
	 * Used for create a response.
	 */
	Task(const std::string& username, const std::string& path, 
         const uint32_t tranID, const uint32_t workerID, 
         const OpCode opCode)
		: username_(username),
          path_(path),
          tranID_(tranID),
		  workerID_(workerID),
          opCode_(opCode)
     {}

	/** 
	* Copy constructor.
	*/
	Task(const Task& other)
    : username_(other.username_),
      path_(other.path_),
      tranID_(other.tranID_),
      workerID_(other.workerID_),
      opCode_(other.opCode_)
    {}

	/**
     * Move constructor.
     */
     Task(Task&& other)
     : username_(std::move(other.username_)),
       path_(std::move(other.path_)),
       tranID_(other.tranID_),
       workerID_(other.workerID_),
       opCode_(other.opCode_)
     {}




   


	inline const std::string& getUsername() const
    {
		return username_;
	}


	inline const std::string& getPath() const
    {
		return path_;
	}


	inline OpType getOpType() const
    {
		return opType_;
	}

    
	inline uint64_t getDataAddress() const
    {
		return dataAddr_;
	}


	inline uint32_t getDataSize() const 
    {
		return dataSize_;
	}


	inline uint32_t getTransctionID() const
    {
		return tranID_;
	}


	inline uint32_t getWorkerID() const
    {
		return workerID_;
	}

	inline OpCode getOpCode() const
    {
		return opCode_;
	}


public:
	std::string username_;
	std::string path_;
	OpType opType_;
	uint64_t dataAddr_;
	uint32_t dataSize_;
	uint32_t tranID_;
	uint32_t workerID_;
	OpCode opCode_;

}; // class Task


} // namesapce singaistorageipc
