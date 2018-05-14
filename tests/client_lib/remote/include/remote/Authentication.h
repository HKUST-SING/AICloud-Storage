#pragma once

// C++ std lib
#include <string>
#include <utility>
#include <cstdint>



// Project lib
#include "include/Task.h"

namespace singaistorageipc
{


class UserAuth
{

  public:


    /*
     * Authentication parameters.
     */
    UserAuth(const char* username, const char* password)
    : username_(username),
      passwd_(password)
     {}


    UserAuth(const std::string& username, 
             const std::string& pass)
    : username_(username),
      passwd_(pass)
     {}
    
    UserAuth(std::string&& username,
             std::string&& pass)
    : username_(std::move(username)),
      passwd_(std::move(pass))
     {}
	
    
      

	/** 
	* Copy constructor.
	*/
	UserAuth(const UserAuth& other)
    : username_(other.username_),
      passwd_(other.passwd_)
    {}

	/**
     * Move constructor.
     */
     UserAuth(UserAuth&& other)
     : username_(std::move(other.username_)),
       passwd_(std::move(other.passwd_))
     {}


    UserAuth& operator=(const UserAuth& other)
    {
      username_ = other.username_;
      passwd_   = other.passwd_;

      return *this;
    }


    UserAuth& operator=(UserAuth&& other)
    {
      username_ = std::move(other.username_);
      passwd_   = std::move(other.passwd_);

      return *this;
    }



public:
	std::string username_; 
	std::string passwd_; // hash of the password

}; // class UserAuth



/*
typedef struct IOResult
{
 
  public:
    enum class Status: uint8_t
    {
      INTERNAL_ERR = 255, // some internal cluster error 
                          // (undo the previous operation)
      SUCCESS      =   0, // successful IO op
      PARTIAL_ERR  =   1, // operation partially completed
      AUTH_ERR     =   2, // machine does not have auth to perform
                          // IO
      
    };


  public:
    IOResult(const UserAuth& user, 
             const IOResult::Status stat, Task::OpType ioType,
             const uint32_t id, const uint32_t remID=0)
    : user_(user),
      ioStat_(stat),
      ioOp_(ioType),
      tranID_(id),
      token_(remID)
    {}
  
 

  public:
    UserAuth     user_; // user which has issues the opearation
    Status     ioStat_; // status of the Io operation
    Task::OpType ioOp_; // IO operation type
    uint32_t   tranID_; // transaction ID for async IO
    uint32_t    token_; // unique identifier for this user of the IO
      

} IOResult; // struct IOResult

*/

} // namesapce singaistorageipc
