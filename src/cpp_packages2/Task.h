/*
 * Define the task structure used in worker interface.
 */

#pragma once

#include <string>
#include <utility>

namespace singaistorageipc{

class Task{

public:
	enum class OpType : uint8_t
	{
		READ = 1,
		WRITE = 2,
		DELETE = 3,
        	CLOSE  = 4
	};

	// result infomation
	enum class OpCode : uint8_t
	{
		OP_SUCCESS       =   0, // operation successful
		OP_ERR_USER      =   1, // user is not authorized to access path
		OP_ERR_PATH      =   2, // no path exists (for READ operations)
		OP_ERR_QUOTA     =   3, // not enough available storage (not implemented yet)
		OP_ERR_LOCK      =   4, // cannot acquire lock for data (retry later)
		OP_ERR_INTERNAL  = 255 // internal error (generic error)
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
		  objSize_(0),
		  workerID_(workerID)

    {}

	/**
	 * Used for create a response.
	 */
	Task(const std::string& username, const std::string& path, 
         const uint32_t tranID, const uint32_t workerID,
		 const uint64_t remSize, const OpCode opCode)
		: username_(username),
          path_(path),
          tranID_(tranID),
		  workerID_(workerID),
		  objSize_(remSize),
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
	  objSize_(other.objSize_),
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
	   objSize_(other.objSize_),
       opCode_(other.opCode_)
     {}




   


	inline uint64_t getUserID() const
    {
		return userID_;
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
	uint64_t userID_;  // global user id used internally within the storage system
	std::string path_;
	OpType opType_;
	uint64_t dataAddr_;
	uint32_t dataSize_;
	uint32_t tranID_;
	uint32_t workerID_;

	/*set by worker*/
	uint64_t objSize_; // remaining bytes to read
	OpCode opCode_;

}; // class Task


} // namesapce singaistorageipc
