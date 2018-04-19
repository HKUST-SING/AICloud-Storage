/*
 * Define the task structure used in worker interface.
 */

#pragma once

#include <string>
#include <utility>



// Project lib
#include "remote/Message.h"
#include "CommonCode.h"

namespace singaistorageipc{

typedef struct Task
{

    // Task fields/atributes
	std::string username_;  // global user id used internally within the storage system
	std::string path_;
	CommonCode::IOOpCode opType_;
	uint64_t dataAddr_;
	uint32_t dataSize_;
	uint32_t tranID_;
	uint32_t workerID_;

	uint64_t objSize_; // used by Unix service and worker to determine object size
	CommonCode::IOStatus opStat_;

    uint32_t mergeID_;

    /**
     * Default constructor.
     */
    Task()
    : username_(std::string("")),
      path_(std::string("")),
      opType_(CommonCode::IOOpCode::OP_NOP),
      dataAddr_(0),
      dataSize_(0),
      tranID_(0),
      workerID_(0),
      objSize_(0),
      opStat_(CommonCode::IOStatus::ERR_INTERNAL)
    {}


	/**
	 * Used for create a Task.
	 * Dedicated for AUTH response
	 */
	Task(const std::string& username, const CommonCode::IOStatus opStat)
		: username_(username),
          path_(std::string("")),
		  opType_(CommonCode::IOOpCode::OP_AUTH),
          dataAddr_(0),
          dataSize_(0),
          tranID_(0),
          workerID_(0),
          objSize_(0),
		  opStat_(opStat)
	{}

	/**
	 * Used for create a request.
	 */
	Task(const std::string& username, const std::string& path, 
        const CommonCode::IOOpCode opType, const uint64_t dataAddr, 
        const uint32_t dataSize, const uint32_t tranID,
		const uint32_t workerID = 0)
		: username_(username),
          path_(path),
          opType_(opType),
		  dataAddr_(dataAddr),
          dataSize_(dataSize),
          tranID_(tranID),
          workerID_(workerID),
		  objSize_(0),
          opStat_(CommonCode::IOStatus::ERR_INTERNAL)

    {}

	/**
	 * Used for create a response.
	 */
	Task(const std::string& username, const std::string& path, 
         const uint32_t tranID, const uint32_t workerID,
		 const uint64_t remSize, const CommonCode::IOOpCode opCode,
         const CommonCode::IOStatus opStat)
		: username_(username),
          path_(path),
          opType_(opCode),
          dataAddr_(0),
          dataSize_(0),
          tranID_(tranID),
		  workerID_(workerID),
		  objSize_(remSize),
          opStat_(opStat)
     {}

	/** 
	* Copy constructor.
	*/
	Task(const struct Task& other)
    : username_(other.username_),
      path_(other.path_),
      opType_(other.opType_),
      dataAddr_(other.dataAddr_),
      dataSize_(other.dataSize_),
      tranID_(other.tranID_),
      workerID_(other.workerID_),
	  objSize_(other.objSize_),
      opStat_(other.opStat_)
    {}

	/**
     * Move constructor.
     */
     Task(struct Task&& other)
     : username_(std::move(other.username_)),
       path_(std::move(other.path_)),
       opType_(other.opType_),
       dataAddr_(other.dataAddr_),
       dataSize_(other.dataSize_),
       tranID_(other.tranID_),
       workerID_(other.workerID_),
	   objSize_(other.objSize_),
       opStat_(other.opStat_)
     {}



   /** 
    * Copy assignment
    */

    Task& operator=(const struct Task& other)
    {
      if(this != &other)
      {
        username_ = other.username_;
        path_     = other.path_;
        opType_   = other.opType_;
        dataAddr_ = other.dataAddr_;
        dataSize_ = other.dataSize_;
        tranID_   = other.tranID_;
        workerID_ = other.workerID_;
        objSize_  = other.objSize_;
        opStat_   = other.opStat_;
      }

      return *this;

    }


   /** 
    * Move assignment
    */
    Task& operator=(struct Task&& other)
    {
      if(this != &other)
      {
        username_ = std::move(other.username_);
        path_     = std::move(other.path_);
        opType_   = other.opType_;
        dataAddr_ = other.dataAddr_;
        dataSize_ = other.dataSize_;
        tranID_   = other.tranID_;
        workerID_ = other.workerID_;
        objSize_  = other.objSize_;
        opStat_   = other.opStat_;
      }

      return *this;

    }

} Task; // struct Task


typedef struct IOResponse
{
  CommonCode::IOOpCode     opType_; // operation type
  CommonCode::IOStatus     opStat_; // operation status (error/success)

  std::unique_ptr<Message> msg_;    // message received from the 
                                    // remote server


  /**
   * Default/Custom constructor for any type of simple
   * initialization.
   */
  IOResponse(std::unique_ptr<Message>&& msgContent = nullptr,
             const CommonCode::IOOpCode opCode = 
                   CommonCode::IOOpCode::OP_NOP,
             const CommonCode::IOStatus opStat =
                   CommonCode::IOStatus::ERR_INTERNAL)
  : opType_(opCode),
    opStat_(opStat),
    msg_(std::move(msgContent))
  {}


  // No copy constructors or copy assignemnts 
  IOResponse(const struct IOResponse&) = delete;
  IOResponse& operator=(const struct IOResponse&) = delete;


  // Support move semantics
  IOResponse(struct IOResponse&& other)
  : opType_(other.opType_),
    opStat_(other.opStat_),
    msg_(std::move(other.msg_))
  {}


  IOResponse& operator=(struct IOResponse&& other)
  {
    if(this != &other)
    {
      // destroy the current message
      msg_.reset(nullptr);

      // move the corresponding values
      opType_ = other.opType_;
      opStat_ = other.opStat_;
      msg_    = std::move(other.msg_);

    }

   
    return *this;
  }



} IOResponse; // response for IO operations


typedef struct IOResult
{

  std::unique_ptr<Request> msg_;  // encoded message for sending 
                                  // to confirm completed WRITE operation
                                  // (COMMIT Operation)

  /**
   * Support simple default constructor.
   */
  IOResult(std::unique_ptr<Request>&& reqMsg = nullptr)
  : msg_(std::move(reqMsg))
  {}


  // Don't support copy semantics.
  IOResult(const struct IOResult&) = delete;
  IOResult& operator=(const struct IOResult&) = delete;

 
  // Support move semantics
  IOResult(struct IOResult&& other)
  : msg_(std::move(other.msg_))
  {
    other.msg_.reset(nullptr); // ensure clean destruction
  }

  IOResult& operator=(struct IOResult&& other)
  {
    if(this != &other)
    {
      // destroy current message
      msg_.reset(nullptr);

      // move values
      msg_.swap(other.msg_);
    }

    return *this;
  }

} IOResult; // struct IOResult

} // namesapce singaistorageipc
