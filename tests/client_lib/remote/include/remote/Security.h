#pragma once

// C++ std lib 
#include <string>
#include <memory>
#include <atomic>
#include <chrono>

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


// forward declaration for security functions
class Security;

namespace securitymodule
{
  // specific functions to the security module.
  void destroySecurity(Security* const obj);

}


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

     typedef struct sec_config_t
     {
       /**
        * A structure used to initialize a security module.
        */

        using TimeUnit = std::chrono::milliseconds;

        static const TimeUnit NO_TIMEOUT;


       /*After how many milliseconds terminate a request*/
       TimeUnit timeout; 
       /* TO BE CONTINUED */

       
       sec_config_t()
       : timeout(NO_TIMEOUT)
       {}

       sec_config_t(const sec_config_t& other)
       : timeout(other.timeout)
       {}

       sec_config_t(sec_config_t&& other)
       : timeout(other.timeout)
       {}

       sec_config_t& operator=(const sec_config_t& other)
       {
         if(this != &other)
         {
           timeout = other.timeout;

         }

         return *this;
 
       }


       sec_config_t& operator=(sec_config_t&& other)
       {
         if(this != &other)
         {
           timeout = other.timeout;
         }

         return *this;
       }


       ~sec_config_t() = default;

     } sec_config_t; // struct sec_config_t


     // for deleteting an object
     using SecurityDeleter = void (*) (Security* const obj);

     Security() = delete;
     Security(const Security&) = delete;
     Security& operator=(const Security&) = delete;

     Security(std::unique_ptr<ServerChannel>&& comm,
              std::unique_ptr<Security::sec_config_t>&& configs,
              std::unique_ptr<SecureKey>&& secretKey = nullptr,
              std::unique_ptr<Cache>&&     userCache = nullptr)
     : done_(true),
       channel_(std::move(comm)),
       confs_(std::move(configs)),
       secret_(std::move(secretKey)),
       cache_(std::move(userCache))
     {
       initConfigs();
     }

     Security(Security&&); // support move constructor
     Security& operator=(Security&&); // support assignment
     
    
       
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
      const auto startVal = done_.load(std::memory_order_acquire);
      if(!startVal) // the service must be started
      {
        done_.store(true, std::memory_order_release); // stop the service
        joinService();                                // wait to complete
      }
    }
   
    /**
     * Initialize the security service.
     * 
     */
    virtual bool initialize()
    { 
      return (channel_ != nullptr);
    }


  /**
   * For starting the service.
   */
   inline void startService()
   {
     done_.store(false, std::memory_order_acquire);
     doStartService(); // start the implementation
   }


  /** 
   * For creating a security module chosen by the client.
   * 
   * @return : unique pointer to a security module 
   */

   static std::shared_ptr<Security> createSharedSecurityModule(
        const char* secType,
        std::unique_ptr<ServerChannel>&& channel,
        std::unique_ptr<Security::sec_config_t>&& configs = nullptr,
        std::unique_ptr<SecureKey>&& secKey = nullptr,
        std::unique_ptr<Cache>&& cache = nullptr);


  protected:
    virtual ~Security() // shall be virtual
    {
       channel_.reset(nullptr); // destroy the managed channel
       confs_.reset(nullptr);   // destroy configurations
       secret_.reset(nullptr);  // destroy the managed secret key
       cache_.reset(nullptr);   // destroy the user data cache
    }


     /**
      * Friend function for destroying an instnace of the
      * Security class.
      */
    friend void securitymodule::destroySecurity(Security* const obj);

    /**
     * For destroyng a Security module.
     * The destructor is made protected in order to
     * avoid destroying an object without calling
     * required steps (the use of the method must
     * ensure clean destruction of the object).
     */
    virtual void destroy() = 0;


    /**
     * For derived classes to initialize their
     * internal structures.
     */
    virtual void doStartService() = 0;

    /** 
     * For joining the service.
     */ 
    virtual void joinService() = 0;


  public:
    std::atomic<bool> done_;                 // if the service is done



  private:
    void initConfigs();                      // initialize configurations


  protected:
    std::unique_ptr<ServerChannel> channel_; // communication channel 
                                             // with the remote server

    std::unique_ptr<Security::sec_config_t> confs_; // configurations

    std::unique_ptr<SecureKey>      secret_; // secret key 
    std::unique_ptr<Cache>           cache_; // user op cache
    

}; // class Security

} // namespace singaistorageipc
