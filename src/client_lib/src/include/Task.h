/*
 * Define the task structure used in worker interface.
 */

#pragma once

#include <string>
#include <utility>

namespace singaistorageipc{

struct Task
{

	enum class OpType : uint8_t
	{
		READ   = 1,
		WRITE  = 2,
		DELETE = 3,
        CLOSE  = 4,
        AUTH   = 5
	};

	// result infomation
	enum class OpCode : uint8_t
	{
		OP_SUCCESS       =   0, // operation successful
		OP_ERR_USER      =   1, // user is not authorized to access path
		OP_ERR_PASSWD	 =	 2, // password is wrong
		OP_ERR_PATH      =   3, // no path exists (for READ operations)
		OP_ERR_QUOTA     =   4, // not enough available storage (not implemented yet)
		OP_ERR_LOCK      =   5, // tried to acquire lock of data and failed multiple times 
        OP_PARTIAL_READ  =   6, // sucessfully read part of the data
                                // ==> issue another READ

		OP_ERR_INTERNAL  = 255  // internal error (generic error)
	};


    // Task fields/atributes
	std::string username_;  // global user id used internally within the storage system
	std::string path_;
	OpType opType_;
	uint64_t dataAddr_;
	uint32_t dataSize_;
	uint32_t tranID_;
	uint32_t workerID_;

	uint64_t objSize_; // used by Unix service and worker to determine object size
	OpCode opCode_;


	/**
	 * Used for create a Task.
	 * Dedicated for AUTH response
	 */
	Task(const std::string& username, OpCode opcode)
		: username_(username),
		  opType_(OpType::AUTH),
		  opCode_(opcode)
	{}

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


}; // struct Task



struct ObjectInfo
{

}; // struct ObjectInfo

} // namesapce singaistorageipc
