
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
#include <deque>
#include <list>


// Facebook folly
#include <folly/futures/Promise.h>
#include <folly/futures/Future.h>


// OpenSSL
#include <openssl/sha.h>

// Ceph libraries
#include <rados/librados.hpp>

// Project

#include "cluster/Worker.h"
#include "cluster/CephContext.h"
#include "include/ConcurrentQueue.h"
#include "include/Task.h"
#include "include/CommonCode.h"
#include "include/JSONHandler.h"
#include "StoreObj.h"


namespace singaistorageipc
{


class StoreWorker: public Worker
{

  private:
  
    using UpperRequest = std::pair<folly::Promise<Task>, Task>;
    using SecResponse  = folly::Future<IOResponse>;

    /**
     * A wrapper for storing a contex of a pending task.
     * An instance of the WorkerContext struct contains a reponse 
     * from the security module and the request from the 
     * upper layer (IPCServer).
     */
    typedef struct WorkerContext
    {
      UpperRequest  uppReq;   // request context     
      SecResponse*  secRes;   // response context

      WorkerContext() = delete;
      WorkerContext(const struct WorkerContext&) = delete;

      WorkerContext(UpperRequest&& passReq,
                    SecResponse&&  recvRes)
      : uppReq(std::move(passReq)),
        secRes(new SecResponse(std::move(recvRes)))
        {}

      // support move operations
      WorkerContext(struct WorkerContext&& other)
      : uppReq(std::move(other.uppReq)),
        secRes(other.secRes)
        {
          // set the pointer of the 'other' object 
          // to nullptr
          other.secRes = nullptr;
        }

      WorkerContext& operator=(struct WorkerContext&& other)
      {
        if(this != &other)
        {
          // move data
          uppReq = std::move(other.uppReq);
          
          // delete my current content
          delete secRes;        

          // swap the pointers
          secRes = other.secRes;
          other.secRes = nullptr; // reset content to nullptr
          
        }

        return *this;
      }

      // destructor
      ~WorkerContext()
      {
        if(secRes)
        {
          delete secRes; // delete the response context
        }
      }
         
      
 
    } WorkerContext; // struct WorkerContext



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

   StoreWorker(std::unique_ptr<CephContext>&& ctx, 
               const uint32_t id,
               std::shared_ptr<Security>&& sec);



    ~StoreWorker() override; // override the destrctor


    virtual bool initialize() override; // initialize the worker

  
    virtual folly::Future<Task> writeStoreObj(const Task& task) override;
    /**
     * Method for writing data to the Ceph cluster.
     * Task encapsulates the data to write and also a result.
     */

    virtual folly::Future<Task> readStoreObj(const Task& task) override;
    /**
     * Interface for reading an object from the storage system.
     * The passed task encapsulates the task (data to read) and
     * also provides an interface for retrieving result.
     */ 

    virtual folly::Future<Task> deleteStoreObj(const Task& task) override;
    /**
     * Interface for deleting an object from the storage system.
     * Since an object may contains multiple Rados objects,
     * this interface deletes multiple Rados objects before
     * returning result.
     */



    virtual folly::Future<Task> completeReadStoreObj(const Task& task) override;
    /**
     * Interface for read completion. Since storage system objects
     * may be extremely large, they may not fit into shared memory
     * or in the machine's memory at all. Due to this, multiple
     * object read operations should be issued.
     */
    


  protected:   
    /**
     * Method which starts the worker. This method is called
     * once the initialized worker has to start processing 
     * tasks.
     */ 
    void processTasks() override;



  private: 
  
    /**
     * Process a READ operation. The method informs the caller
     * about the result through the passed promise object.
     *
     * @param: task: IO to perform and promise to fulfill
     */
     void processReadOp(UpperRequest&& task);


     /**
      * Issue a new READ operation to an authentication server.
      * This method is called if the request is to read a
      * piece of data (it has not been issued before).
      */
     void issueNewReadOp(UpperRequest&& newTask);


    /**
     * Process a WRITE operation. The method informs the caller
     * about the result through the passed promise object.
     *
     * @param: task: IO to perform and promise to fulfill
     */
     void processWriteOp(UpperRequest&& task);

    /**
     * Process a DELTE operation. The method informs the caller
     * about the result through the passed promise object.
     *
     * @param: task: IO to perform and promise to fulfill
     */
     void processDeleteOp(UpperRequest&& task);


     /** 
      * Return data from a local buffer 
      * (write into the shared memory) from local memory
      */
     void handlePendingRead(
          std::map<uint32_t, StoreObj>::iterator& mapItr,
          UpperRequest& task);



    /**
     * Method goes over all already issued operations
     * and starts processing them.
     */
    void executeRadosOps();

    /** 
     * Terminate the worker. Worker cleans itslef up
     * and terminates
     */
    void closeStoreWorker();


  private:
    ConcurrentQueue<UpperRequest, std::deque<UpperRequest> > tasks_; 
                             // the queue is accessed 
                             // by two threads
                             // as the producer provides tasks, 
                             // the consumer servs them.


    // Map of sent requests
    std::list<WorkerContext> responses_;     // issued requests

    std::map<uint32_t, StoreObj> pendReads_; // completed remotely 
                                             // tasks, pending for 
                                             // further processing
                                             // locally



    unsigned char workerSecret[SHA256_DIGEST_LENGTH]; // shared
                                                      // secret
                                                      // between
                                                      // the workers
                                                      // the auth server
                                             

    std::unique_ptr<CephContext> cephCtx_;  // for ceph communication  
                                            // (accesing the Cluster)
}; // class StoreWorker


} // namesapce singaistorageaipc
