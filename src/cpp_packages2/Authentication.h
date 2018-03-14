#pragma once

// C++ std lib
#include <string>
#include <utility>
#include <cstdlib>



// Project lib
#include "Task.h"

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



typedef struct AuthenticationResponse
{
  public:
    enum class AuthCode
    {
      INTERNAL_ERR = -1, // some system internal error
      SUCCESS      =  0, // successful authentication
      USER_ERR     =  1, // no such user found
      PASSWD_ERR   =  2  // wrong user password
      
    };
    
  UserAuth   user_;     // encapsulate the user
  AuthCode   authCode_; // stores the authentication result
  uint32_t   tranID_;   // transaction ID

  AuthenticationResponse(const UserAuth& user,
                         const AuthCode code,
                         const uint32_t opID)
  : user_(user),
    authCode_(code),
    tranID_(opID)
  {}


  /**
   * Default constructor.
   */
  AuthenticationResponse()
  : user_("", ""),
    authCode_(AuthCode::INTERNAL_ERR),
    tranID_(0)
  {}


  AuthenticationResponse(
    const struct AuthenticationResponse& other)
  : user_(other.user_),
    authCode_(other.authCode_),
    tranID_(other.tranID_)
   {}

  AuthenticationResponse(
    struct AuthenticationResponse&& other)
  : user_(std::move(other.user_)),
    authCode_(other.authCode_),
    tranID_(other.tranID_)
   {}


  struct AuthenticationResponse& operator=(
         const struct AuthenticationResponse& other)
  {
    user_     = other.user_;
    authCode_ = other.authCode_;
    tranID_   = other.tranID_;

    return *this;
  }


  struct AuthenticationResponse& operator=(
         AuthenticationResponse&& other)
  {
    user_     = std::move(other.user_);
    authCode_ = other.authCode_;
    tranID_   = other.tranID_;

    return *this;
  }


} AuthenticationResponse; // struct



typedef struct OpPermissionResponse
{
  public:
    enum class PermCode
    {
      INTERNAL_ERR = -1, // some system internal error
      SUCCESS      =  0, // operation has been approved
      USER_ERR     =  1, // user cannot perform operation
                         // on the data (ACL problem)
      PATH_ERR     =  2, // no such path exists 
      QUOTA_ERR    =  3  // user has axceeded data quota
      
    };
    
  UserAuth      user_;     // user 
  PermCode      permCode_; // stores the permission result
  Task::OpType  ioOp_;     // operation type
  uint32_t      tranID_;   // message id

  OpPermissionResponse(const UserAuth& user, const PermCode code,
                       const Task::OpType ioType,
                       const uint32_t opID)
  : user_(user),
    permCode_(code),
    ioOp_(ioType),
    tranID_(opID)
  {}


  // other constructors
  OpPermissionResponse()
  : user_("", ""),
    permCode_(PermCode::INTERNAL_ERR),
    ioOp_(Task::OpType::CLOSE),
    tranID_(0)
  {}

  OpPermissionResponse(
    const struct OpPermissionResponse& other)
  : user_(other.user_),
    permCode_(other.permCode_),
    ioOp_(other.ioOp_),
    tranID_(other.tranID_)
  {}


  OpPermissionResponse(
    struct OpPermissionResponse&& other)
  : user_(std::move(other.user_)),
    permCode_(other.permCode_),
    ioOp_(other.ioOp_),
    tranID_(other.tranID_)
  {}



  OpPermissionResponse& operator=(
    const struct OpPermissionResponse& other)
  {
    user_     = other.user_;
    permCode_ = other.permCode_;
    ioOp_     = other.ioOp_;
    tranID_   = other.tranID_;

    return *this;
  }

 
  OpPermissionResponse& operator=(
    struct OpPermissionResponse&& other)
  {
    user_     = std::move(other.user_);
    permCode_ = other.permCode_;
    ioOp_     = other.ioOp_;
    tranID_   = other.tranID_;

    return *this;
  }


} OpPermissionResponse; // struct



} // namesapce singaistorageipc
