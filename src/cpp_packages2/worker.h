
#pragma once

#include <mutex>
#include <condition_variable> 
#include <vector>
#include <utility>
#include <cstdint>


#include "store_obj.h"


namespace singaistorageipc
{


class Worker
{

  private:
    std::mutex initLock_;
    std::condition_variable initCond_;
    bool init_ = false;
    std::atomic<unsigned int> norefs_;
    unsigned int id_; 
    std::vector< std::pair<uint32_t, StoreObj> > pendObjs_;

    Security* secure_; // pointer to the security module


  public:

    bool done_ = false; // if the worker has completed its work

    /** 
     * A worker interface which executes Ceph requests on behalf
     * of the ipc service. The worker provides a few methods for
     * the service to communicate with it and a few methods
     * for the security service to interact with the worker.
     */

    Worker()=delete;   // no default constructor
    Worker(const Worker&) = delete; // no copy constructor
    Worker& operator=(const Worker&) = delete; // no assigment

    
    Worker(const unsigned int id, Security* sec);

    virtual ~Worker(); // destructor shall be virtual


    virtual void initialize() {} // initialize the worker
    virtual void destroy() {}    // destroy the worker
  
    virtual Future<Task> writeStoreObj(const Task& task) = 0;
    /**
     * Method for writing data to the Ceph cluster.
     * Task encapsulates the data to write and also a result.
     */

    virtual Future<Task> readStoreObj(const Task& task) = 0;
    /**
     * Interface for reading an object from the storage system.
     * The passed task encapsulates the task (data to read) and
     * also provides an interface for retrieving result.
     */ 

    virtual Future<Task> deleteStoreObj(const Task& task) = 0;
    /**
     * Interface for deleting an object from the storage system.
     * Since an object may contains multiple Rados objects,
     * this interface deletes multiple Rados objects before
     * returning result.
     */



    virtual Future<Task> completeReadStoreObj(const Task& task) = 0;
    /**
     * Interface for read completion. Since storage system objects
     * may be extremely large, they may not fit into shared memory
     * or in the machine's memory at all. Due to this, multiple
     * object read operations should be issued.
     */
     


    inline unsigned int getWorkerId() const
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
        init_cond.wait(tmp);
      }
    }
  

}; // class Worker


} // namesapce singaistorageaipc
