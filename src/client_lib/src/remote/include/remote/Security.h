#pragma once

// C++ std lib 
#include <string>
#include <memory>
#include <atomic>

// Facebook folly
#include <folly/futures/Future.h>


// Project lib
#include "include/CommonCode.h"
#include "include/Task.h"
#include "Authentication.h"
#include "Message.h"
#include "SecureKey.h"
#include "ServerChannel.h"
#include "Cache.h"


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

     Security() = delete;
     Security(const Security&) = delete;
     Security& operator=(const Security&) = delete;

     Security(std::unique_ptr<ServerChannel>&& comm,
              std::unique_ptr<SecureKey>&& secretKey = nullptr,
              std::unique_ptr<Cache>&&     userCache = nullptr)
     : done_(false),
       channel_(std::move(comm)),
       secret_(std::move(secretKey)),
       cache_(std::move(userCache))
     {}

     Security(Security&&); // support move constructor
     Security& operator=(Security&&); // support assignment
     
    
     virtual ~Security() // shall be virtual
     {
       channel_.reset(nullptr); // destroy the managed channel
       secret_.reset(nullptr);  // destroy the managed secret key
       cache_.reset(nullptr);   // destroy the user data cache
     }

  
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

     virtual folly::Future<Task> 
             clientConnect(const UserAuth& user) = 0;

     /**
     * Check user IO interface to the storage service. 
     * This method is used for checking if the given user
     * has credentials to perform IO to the path and
     * if the path exists.
     *
     * @param: path:     storage path for accesing data
     * @param: user:     user credentials 
     * @param: op:       IO operation type 
     * @param: dataSize: (only for write) write request size
     *
     * @return: folly::Future for async computation and eventual
     *          result of the request
     */

     virtual folly::Future<IOResponse> 
             checkPerm(const std::string& path,
                       const UserAuth& user,
                       const CommonCode::IOOpCode op,
                       const uint64_t dataSize=0) = 0;  


	/** 
	 * Interface for sending a request to retrieve the information
	 * about the storage system object identified by the path.
     * The method should return information which could help
     * read/write data and also after returning, on success, 
     * the caller has right to read/write the data to path.
     *
     * @param path: identifier of a data object to access
	 * @param op  : IO operation to be performed on data
	 */

	/*virtual folly::Future<ObjectInfo> 
		lockAndRetrieveInfo(const std::string& path,
							const Task::OpType op) = 0;
     */

     /**
      * For sending an IO operation result.
      * 
      * @param: IO operation status and user info
      *
      * @return: async interface for waiting reponse
      */

     virtual folly::Future<IOResponse>
             sendIOResult( const std::string& path,
                           const UserAuth& user,
                           const CommonCode::IOOpCode op,
                           const IOResult& res) = 0;


     /**
      * Same as obove method, only this one passes
      * full control of the response to the  
      * security module.
      */
     virtual folly::Future<IOResponse>
             sendIOResult( std::string&& path,
                           UserAuth&& user,
                           const CommonCode::IOOpCode op,
                           IOResult&& res) = 0;


    /**
     * For stopping the service. 
     */

    inline void stopService(void)
    {
      done_.store(true); // stop the service
    }
   
    /**
     * Initialize the security service.
     * 
     */
    virtual bool initialize()
    { 
      return true;
    }


  /**
   * For starting the service.
   */
   virtual void startService() = 0;

  /** 
   * For joining the service.
   */ 
  virtual void joinService()
  {}
  


  /** 
   * For creating a security module chosen by the client.
   * 
   * @return : unique pointer to a security module 
   */

   static std::shared_ptr<Security> createSecurityModule(
        const char* secType,
        std::unique_ptr<ServerChannel>&& channel,
        std::unique_ptr<SecureKey>&& secKey = nullptr,
        std::unique_ptr<Cache>&& cache = nullptr);





  public:
    std::atomic<bool> done_;                 // if the service is done


  protected:
    std::unique_ptr<ServerChannel> channel_; // communication channel 
                                             // with the remote server
    std::unique_ptr<SecureKey>      secret_; // secret key 
    std::unique_ptr<Cache>           cache_; // user op cache
    

}; // class Security

} // namespace singaistorageipc
