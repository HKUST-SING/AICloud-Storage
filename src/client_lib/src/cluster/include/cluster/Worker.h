
#pragma once


// C++ std lib
#include <mutex>
#include <condition_variable> 
#include <atomic>
#include <cstdint>
#include <memory>


// Facebook folly lib
#include <folly/futures/Future.h>


// Project lib
#include "remote/Security.h"
#include "include/Task.h"
#include "RemoteProtocol.h"
#include "CephContext.h"


// forward declaration
class CephContext;

namespace singaistorageipc
{


class Worker
{

  public:

    /** 
     * A worker interface which executes Ceph requests on behalf
     * of the ipc service. The worker provides a few methods for
     * the service to communicate with it and a few methods
     * for the security service to interact with the worker.
     */

    Worker()=delete;   // no default constructor
    Worker(const Worker&) = delete; // no copy constructor
    Worker& operator=(const Worker&) = delete; // no assigment
    Worker(Worker&&) = delete;          // no  move constructor 
    Worker& operator=(Worker&&)=delete; // no move assignment
    

    Worker(const uint32_t id, std::shared_ptr<Security>&& sec)
    : done_(false),
      id_(id),
      secure_(std::move(sec))
    {}

    Worker(const uint32_t id, std::shared_ptr<Security> sec)
    : done_(false),
      id_(id),
      secure_(sec)
    {}
    

    virtual ~Worker() // destructor shall be virtual
    {
     secure_ = nullptr; // reduce the number of references
    } 


    virtual bool initialize() 
    {
      return true;
    } // initialize the worker


    inline void stopWorker() 
     {
      const bool curDone = done_.load(std::memory_order_acquire);
      
      if(!curDone)
      {
        done_.store(true, std::memory_order_release); // stop the worker
        joinWorker();
      }

     } 
              
  
    virtual folly::Future<Task> writeStoreObj(const Task& task) = 0;
    /**
     * Method for writing data to the Ceph cluster.
     * Task encapsulates the data to write and also a result.
     */

    virtual folly::Future<Task> readStoreObj(const Task& task) = 0;
    /**
     * Interface for reading an object from the storage system.
     * The passed task encapsulates the task (data to read) and
     * also provides an interface for retrieving result.
     */ 

    virtual folly::Future<Task> deleteStoreObj(const Task& task) = 0;
    /**
     * Interface for deleting an object from the storage system.
     * Since an object may contains multiple Rados objects,
     * this interface deletes multiple Rados objects before
     * returning result.
     */



    virtual folly::Future<Task> completeReadStoreObj(const Task& task) = 0;
    /**
     * Interface for read completion. Since storage system objects
     * may be extremely large, they may not fit into shared memory
     * or in the machine's memory at all. Due to this, multiple
     * object read operations should be issued.
     */


    virtual folly::Future<Task> abortStoreObj(const Task& task) = 0;
    /**
     * Interface for aborting an operation on an object. It may 
     * happen that reading/writing fails on client side, so 
     * this interface allows the worker to release all preallocated
     * resources for an IO operation.
     */


    virtual folly::Future<Task> closeStoreObj(const Task& task) = 0;
    /**
     * Interface for closing a user context. When this gets called,
     * the operations related to one user/socket id are released
     * terminated.
     */


    inline uint32_t getWorkerId() const
    /** Return a unique worker ID within the same program.
     *
     */    

    {
      return id_;
    }


    inline void releaseWorker()
    {
      norefs_.fetch_sub(1); // reduce the number of references
    }

    
    inline void initDone()
    /**
     * Initialization of the worker is done.
     * After this method call, using the worker is safe.
     */
    {
      std::lock_guard<std::mutex> tmp(initLock_);
      init_ = true;           // initialized

      initCond_.notify_all(); // notify all threads that waiting 
                              // for this worker to be initialized      
    }
    


    inline bool isInit() const
    {
      std::lock_guard<std::mutex> tmp(initLock_);
      return init_;
    }


    inline void waitForInit()
    /**
     * If the worker is initialized in a different thread,
     * then this method ensures that the current thrad 
     * will not use the uninitilized worker.
     */
    {
      std::unique_lock<std::mutex> tmp(initLock_);
      
      while(!init_) // wait for the worker to be initialized
      {
        initCond_.wait(tmp);
      }
    }


    /** 
     * The running method of the worker. The method is the core
     * of the worker since it defines the work. Implementations
     * choose how to handle the tasks.
     *
     */
    virtual void processTasks() = 0;   


    std::atomic<bool> done_; // if the worker has completed its work


    /**
     * The factory method creates a worker per request.
     * 
     * @param type : worker type
     * @param ctx  : CephContext for the worker to use
     * @param id   : unique worker id
     * @param sec  : a reference to a Security module 
     *
     * @return : worker
     */
    static std::unique_ptr<Worker> createRadosWorker(const char* type,
                                     std::unique_ptr<CephContext>&& ctx,
                                     std::unique_ptr<RemoteProtocol>&& prot,
                                     const uint32_t id,
                                     std::shared_ptr<Security> sec);


    /**
     * The factory method creates a worker per request.
     * 
     * @param type : worker type
     * @param ctx  : CephContext for the worker to use
     * @param id   : unique worker id
     * @param sec  : a reference to a Security module 
     *
     * @return : worker
     */
    static std::unique_ptr<Worker> createRadosWorker(
                                     const std::string& type,
                                     std::unique_ptr<CephContext>&& ctx,
                                     std::unique_ptr<RemoteProtocol>&& prot,
                                     const uint32_t id,
                                     std::shared_ptr<Security> sec);


  protected:
    /**
     * Method for subclasses to implement to terminate
     * a worker in a clean way.
     */
    virtual void joinWorker()
    {}


  protected:
    mutable std::mutex initLock_;
    mutable std::condition_variable initCond_;
    bool init_ = false;
    std::atomic<unsigned int> norefs_;
    uint32_t id_; 
    std::shared_ptr<Security> secure_; // pointer to the security module
  

}; // class Worker


} // namesapce singaistorageaipc
