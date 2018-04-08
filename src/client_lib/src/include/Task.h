/*
 * Define the task structure used in worker interface.
 */

#pragma once

#include <string>
#include <utility>



// Project lib
#include "CommonCode.h"
#include "remote/Message.h"
#include "remote/Authentication.h"

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


	/**
	 * Used for create a Task.
	 * Dedicated for AUTH response
	 */
	Task(const std::string& username, const CommonCode::IOStatus opstat)
		: username_(username),
		  opType_(CommonCode::IOOpCode::OP_AUTH),
		  opStat_(opstat)
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
		  objSize_(0),
		  workerID_(workerID)

    {}

	/**
	 * Used for create a response.
	 */
	Task(const std::string& username, const std::string& path, 
         const uint32_t tranID, const uint32_t workerID,
		 const uint64_t remSize, const CommonCode::IOStatus opStat)
		: username_(username),
          path_(path),
          tranID_(tranID),
		  workerID_(workerID),
		  objSize_(remSize),
          opStat_(opStat)
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
      opStat_(other.opStat_)
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
       opStat_(other.opStat_)
     {}

} Task; // struct Task


typedef struct IOResponse
{
  CommonCode::IOOpCode opType_; // operation type
  CommonCode::IOStatus opStat_; // operation status (error/success)

  Message              msg_;   // message received from the 
                               // remote server

} IOResponse; // response for IO operations


typedef struct IOResult
{

  CommonCode::IOOpCode opType_; // operation type
  UserAuth user_; // for authneticating user 
  Request msg_; // encoded message for sending 
                 // to confirm completed WRITE operation

} IOResult; // struct IOResult

} // namesapce singaistorageipc
