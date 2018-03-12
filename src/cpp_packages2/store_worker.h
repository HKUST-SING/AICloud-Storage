
#pragma once

// C++ std
#include <mutex>
#include <condition_variable> 
#include <vector>
#include <utility>
#include <cstdint>
#include <map>
#include <cstdint>


// Facebook folly
#include <folly/futures/Promise.h>


// Ceph libraries
#include <rados/librados.hpp>

// Project
#include "store_obj.h"
#include "worker.h"
#include "concurrent_queue.h"

namespace singaistorageipc
{


class StoreWorker: public Worker
{

  public:


    struct CephComm
    {
      librados::Rados cluster_; // one cluster at most
      std::string clusterName_; // cluster name
      std::string  accessName_; // user name used for accessing
                                // the Ceph storage cluster
      uint64_t    accessFlags_; // falgs for Ceph usage
      std::map<std::string, librados::IoCtx> ioOps_; 
                                           // references to possible
                                           // communications
    }



    /** 
     * First rough implmentaiton of the storage worker. This worker
     * provides no gurantee that data has been stored proerly, or 
     * consistency guarantee. It only reads/writes data from/to
     * a Ceph storage cluster.
     */

    StoreWorker()=delete;   // no default constructor
    StoreWorker(const Worker&) = delete; // no copy constructor
    StoreWorker& operator=(const Worker&) = delete; // no assigment

    
    StoreWorker(const unsigned int id, Security* sec);

    virtual ~StoreWorker(); // destructor shall be virtual


    virtual void initialize() override; // initialize the worker
    virtual void destroy() override;    // destroy the worker
  
    virtual Future<Task> writeStoreObj(const Task& task) override;
    /**
     * Method for writing data to the Ceph cluster.
     * Task encapsulates the data to write and also a result.
     */

    virtual Future<Task> readStoreObj(const Task& task) override;
    /**
     * Interface for reading an object from the storage system.
     * The passed task encapsulates the task (data to read) and
     * also provides an interface for retrieving result.
     */ 

    virtual Future<Task> deleteStoreObj(const Task& task) override;
    /**
     * Interface for deleting an object from the storage system.
     * Since an object may contains multiple Rados objects,
     * this interface deletes multiple Rados objects before
     * returning result.
     */



    virtual Future<Task> completeReadStoreObj(const Task& task) override;
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
      return Worker::id_;
    }



    /** These methods should not be used by direct instantiations
     *  of the StoreWorker class. The user should use Worker pointers
     *  since these methods are not virtual an only applicable to 
     *  Workers.
     */
    inline void releaseWorker()
    {
      Worker::releaseWorker();
    }


    inline void initDone()
    {
      Worker::initDone();
    }   


    inline bool isInit() const
    {
      return Worker::isInit();
    }


    inline void waitForInit()
    {
      Worker::waitForInit();
    }



  private:
    ConcurrentQueue<std::pair<folly::Promise<Task>, const Task&> > tasks_; 
                             // the queue is accessed 
                             // by two threads
                             // as the producer provides tasks, 
                            // the consumer servs them.


    std::map<uint32_t, StoreObj> pendTasks_; // completed remotely 
                                             // tasks, pending for 
                                             // further processing
                                             // locally


   ceph::libardos:
  

}; // class Worker


} // namesapce singaistorageaipc
