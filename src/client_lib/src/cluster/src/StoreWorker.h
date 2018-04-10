
#pragma once

// C++ std
#include <mutex>
#include <condition_variable> 
#include <vector>
#include <utility>
#include <cstdint>
#include <map>
#include <cstdint>
#include <memory>


// Facebook folly
#include <folly/futures/Promise.h>
#include <folly/futures/Future.h>

// Ceph libraries
#include <rados/librados.hpp>

// Project

#include "cluster/Worker.h"
#include "cluster/CephContext.h"
#include "include/ConcurrentQueue.h"
#include "include/Task.h"
#include "include/CommonCode.h"
#include "StoreObj.h"


namespace singaistorageipc
{


class StoreWorker: public Worker
{

  public:

    /** 
     * First rough implmentaiton of the storage worker. This worker
     * provides no gurantee that data has been stored proerly, or 
     * consistency guarantee. It only reads/writes data from/to
     * a Ceph storage cluster.
     */

    StoreWorker()=delete;   // no default constructor
    StoreWorker(const StoreWorker&) = delete; // no copy constructor
    StoreWorker& operator=(const StoreWorker&) = delete; // no assigment
    StoreWorker(StoreWorker&&) = delete; // cannot move
    StoreWorker& operator=(StoreWorker&&) = delete; // cannot move
    

   StoreWorker(const CephContext& ctx, const uint32_t& id, 
                std::shared_ptr<Security>&& sec);



    ~StoreWorker() override; // override the destrctor


    virtual bool initialize() override; // initialize the worker

  
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
     


    inline uint32_t getWorkerId() const
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


    virtual void processTasks() override;



  private: 
  
    /**
     * Process a READ operation. The method informs the caller
     * about the result through the passed promise object.
     *
     * @param: task: IO to perform and promise to fulfill
     */
     void processReadOp(
          std::pair<folly::Promise<Task>, const Task&>& task);


    /**
     * Process a WRITE operation. The method informs the caller
     * about the result through the passed promise object.
     *
     * @param: task: IO to perform and promise to fulfill
     */
     void processWriteOp(
          std::pair<folly::Promise<Task>, const Task&>& task);

    /**
     * Process a DELTE operation. The method informs the caller
     * about the result through the passed promise object.
     *
     * @param: task: IO to perform and promise to fulfill
     */
     void processDeleteOp(
          std::pair<folly::Promise<Task>, const Task&>& task);


     /** 
      * Return data from a local buffer 
      * (write into the shared memory) from local memory
      */
     void handlePendingRead(
          std::map<uint32_t, StoreObj>::iterator&,
          std::pair<folly::Promise<Task>, const Task&>&);


  private:
    ConcurrentQueue<std::pair<folly::Promise<Task>, const Task&> > tasks_; 
                             // the queue is accessed 
                             // by two threads
                             // as the producer provides tasks, 
                            // the consumer servs them.


    std::map<uint32_t, StoreObj> pendReads_; // completed remotely 
                                             // tasks, pending for 
                                             // further processing
                                             // locally

    //std::map<const std::string, > pendOps_; // active ops for seq

    uint32_t                     tranID_;    // unique key that 
                                             // allows to identify
                                             // worker ops with
                                             // security module

    CephContext cephCtx_;                   // for ceph communication  
                                            // (accesing the Cluster)
}; // class Worker


} // namesapce singaistorageaipc
