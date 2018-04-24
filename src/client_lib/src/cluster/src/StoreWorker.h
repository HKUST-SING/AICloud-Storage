
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
#include <functional>


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
#include "cluster/RemoteProtocol.h"
#include "include/ConcurrentQueue.h"
#include "include/Task.h"
#include "include/CommonCode.h"
#include "DataObject.h"



namespace singaistorageipc
{


namespace radosbuff = radosbuffermanagement;

class StoreWorker: public Worker
{

  private:
  

    /*useful definitions*/
    using UpperRequest = std::pair<folly::Promise<Task>, Task>;
    using SecResponse  = folly::Future<IOResponse>;
    using Status       = CommonCode::IOStatus;
    using OpCode       = CommonCode::IOOpCode;


    /**
     * A wrapper for storing a contex of a pending task.
     * An instance of the WorkerContext struct contains a reponse 
     * from the security module and the request from the 
     * upper layer (IPCServer).
     */
    typedef struct WorkerContext
    {
      UpperRequest*  uppReq;   // request context     
      SecResponse*   secRes;   // response context


      // no copying supported
      WorkerContext(const struct WorkerContext&) = delete;
      WorkerContext& operator=(const struct WorkerContext&) = delete;


      /**
       * Default Constructor.
       */
      WorkerContext()
      : uppReq(nullptr),
        secRes(nullptr)
      {}

      WorkerContext(UpperRequest&& passReq,
                    SecResponse&&  recvRes)
      : uppReq(new UpperRequest(std::move(passReq))),
        secRes(new SecResponse(std::move(recvRes)))
        {}

      // support move operations
      WorkerContext(struct WorkerContext&& other)
      : uppReq(other.uppReq),
        secRes(other.secRes)
        {
          // set the pointer of the 'other' object 
          // to nullptr
          other.uppReq = nullptr;
          other.secRes = nullptr;
        }

      WorkerContext& operator=(struct WorkerContext&& other)
      {
        if(this != &other)
        {

          // delete current content
          if(secRes)
          {
            delete secRes;
          }

          if(uppReq)
          {
            delete uppReq;
          }  

          // steal pointers
          uppReq  = other.uppReq;
          secRes = other.secRes;

          // reset the pointers of 'other'
          other.uppReq = nullptr;
          other.secRes = nullptr;
          
        }

        return *this;
      }

      // destructor
      ~WorkerContext()
      {
        if(secRes)
        {
          delete secRes; // delete the response context
          secRes = nullptr;
        }

        if(uppReq)
        {
          delete uppReq; // delete the upper request
          uppReq = nullptr;
        }
      }
          
    } WorkerContext; // struct WorkerContext


    typedef struct OpContext
    {
      std::shared_ptr<RemoteProtocol::ProtocolHandler>   prot;
      radosbuff::DataObject                            object;
      uint32_t                                         tranID;
      uint32_t                                         backID;
      uint64_t                                         totalOpSize;
      std::list<std::pair<uint32_t, librados::bufferlist>> pendData;
      std::list<UpperRequest> pendRequests; // used by replies
      

      OpContext();
      OpContext(struct OpContext&&);     
      OpContext(std::shared_ptr<RemoteProtocol::ProtocolHandler> ph,
                radosbuff::DataObject&& ob);

      OpContext(const struct OpContext&) = delete;
      OpContext& operator=(const struct OpContext&) = delete;
      OpContext& operator=(struct OpContext&&) = delete;

      ~OpContext();

     
       bool insertReadBuffer(librados::bufferlist&& buffer, 
                        const uint32_t id);


       bool appendRequest(UpperRequest&& val);

       void closeContext();

       private:
         bool active;

    } OpContext; // struct OpContext


    typedef struct UserCtx
    {
      std::string path; // data path (key)
      uint32_t    opID; // Rados operation id

      UserCtx()
      : path(""), 
        opID(0)
      {}
      
      UserCtx(const std::string& pVal, const uint32_t tranID = 0)
      : path(pVal),
        opID(tranID)
      {}

       
      ~UserCtx() = default;
    } UserCtx; // struct UserCtx


    using ProtocolRes  =\
        std::pair<std::shared_ptr<RemoteProtocol::ProtocolHandler>,\
                                  WorkerContext*>;

    using OpItr = std::map<std::string, StoreWorker::OpContext>::iterator;

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
    

    StoreWorker(std::unique_ptr<CephContext>&&    ctx,
                std::unique_ptr<RemoteProtocol>&& prot, 
                const uint32_t id,
                std::shared_ptr<Security> sec);



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
      * Process a WRITE_CHECK operation. The method informs
      * the caller about the result through the passed
      * promise object.
      *
      * @param: task: IO to perform and promise to fulfill
      */
     void processNewWriteOp(UpperRequest&& task);

     


    
     /**
      * Issue an IO operation request to the remote server.
      * The operation is only used for later respones.
      *
      * @param task    : a request issued by user
      * @param optype  : IO operation type
      *
      */
     void sendRequestToServer(UpperRequest&& task, const OpCode opType);


     /**
      * Helper function for handling errors to the user.
      *
      * @param req  : refernece to the user context 
      * @param stat : error code number
      */
     inline void notifyUserError(UpperRequest& req, 
                                 const Status stat = Status::ERR_INTERNAL);


      /**
       * Method handles operation contexts. When a new 
       * operation is being issued to a remote server,
       * a context might need to be created to ensure
       * serializability.
       *
       * @param : pathVal : operation data path
       * @param : opType  : operation type
       *
       * @return : possible to send the request
       */
      bool createOperationContext(const std::string& pathVal,
                                  const OpCode opType);

    /** 
     * Terminate the worker. Worker cleans itslef up
     * and terminates
     */
    void closeStoreWorker();


    /**
     * Check if pending tasks are empty.
     *
     * @return: true if there are no pending tasks
     */

     bool pendTasksEmpty() const;


    /** 
     * Check if any process has been done with currently
     * active Rados operations 
     * (if any of them has completed).
     */
     void handleActiveOps();


    /**
     * Check if any remote responses have to be processed.
     */
     void handleRemoteResponses();


    /**
     * Check if need to issue any pending task.
     * Operations proceed if no active operations
     * to the same path.
     */
     void handlePendingTasks();


     /**
      * Process completed Rados Write operation.
      */
      void processCompletedRadosWrite(CephContext::RadosOpCtx* const);

     /**
      * Process completed Rados Read operation.
      */
      void processCompletedRadosRead(CephContext::RadosOpCtx* const);

     /**
      * Process pending read operations (checking is already done).
      *
      */
      void processPendingRead(OpItr& iter);
   


     /**
      * Try to update Write operation context
      */
      void updateWriteOpContext(OpItr& iter);

     /**
      * Issue a write operation
      */
      void issueRadosWrite(OpItr& iter);

     /**
      * Send information to the remote server about successful
      * write operation.
      */ 
      void sendWriteConfirmation(OpItr&);

     /** 
      * Decipher the remote operation and decide
      * how a response from a remote server has to be
      * processed locally by a store worker.
      */
       void processRemoteResult(std::list<WorkerContext>::iterator&,
                                std::shared_ptr<RemoteProtocol::ProtocolHandler>); 
 

  private:
    ConcurrentQueue<UpperRequest, std::deque<UpperRequest> > tasks_; 
                             // the queue is accessed 
                             // by two threads
                             // as the producer provides tasks, 
                             // the consumer servs them.


    // Structures for handling
    // remote requests
    std::list<WorkerContext>    responses_;     // issued requests
 


    // serializes WRITES and READS to the same path
    std::map<std::string, std::deque<UpperRequest> > pendTasks_;
    std::map<std::string, OpContext>                 activeOps_;    


    unsigned char workerSecret[SHA256_DIGEST_LENGTH + 1]; 
                                                      // shared
                                                      // secret
                                                      // between
                                                      // the workers
                                                      // and the auth
                                                      // server
                                             

    CephContext* cephCtx_;     // for ceph communication  
                               // (accesing the Cluster)

    RemoteProtocol* remProt_;  // remote protocol
}; // class StoreWorker


} // namesapce singaistorageaipc
