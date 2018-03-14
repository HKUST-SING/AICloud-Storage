#pragma once

// C++ std lib 
#include <string>
#include <memory>

// Facebook folly
#include <folly/futures/Future.h>


// Project lib
#include "Authentication.h"
#include "Task.h"
#include "ServerChannel.h"

namespace singaistorageipc
{

/*************************************************************
*   |   |     |---  |---    
*   |   |     |     |---     DISRUPTOR PATTERN
*   |___|   \_|     |---
**************************************************************/

class Security
{

  /** 
   * Interface for the security module of the storage service.
   * The interface defines the protocol of communication between
   * the IPC module of the storage service and the security module.
   * The interface also provides the interface for workers to 
   * with the module.
   */


  public:
  
    /**
     * Connection interface to the storage service. 
     * This method is used to perform initial user authentication
     * steps. 
     *
     * @param: user: user credentials used for authentication
     *
     * @return: folly::Future for async computation and eventual
     *          result of the request
     */

     virtual folly::Future<AuthenticationResponse> 
             clientConnect(const UserAuth& user) = 0;

     /**
     * Check user IO interface to the storage service. 
     * This method is used for checking if the given user
     * has credentials to perform IO to the path and
     * if the path exists.
     *
     * @param: path: storage path for accesing data
     * @param: user: user credentials 
     * @param: op:   IO operation type 
     *
     * @return: folly::Future for async computation and eventual
     *          result of the request
     */

     virtual folly::Future<OpPermissionResponse> 
             checkPerm(const std::string& path,
                       const UserAuth& user,
                       const Task::OpType op) = 0;   
    



  private:
    std::unique_ptr<ServerChannel> channel_; // communication channel 
                                             // with the remote server
    

}; // class Security

} // namespace singaistorageipc
