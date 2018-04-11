// C++ std
#include <cassert>
#include <cstring>


// Facebook folly



// Project
#include "StoreWorker.h"
#include "remote/Authentication.h"

#define WORKER_SHARED_SECRET "workersharedsecretpassword"

using folly::Future;
using folly::Promise;

namespace singaistorageipc
{

  
StoreWorker::StoreWorker(const CephContext& ctx, const uint32_t& id, 
                std::shared_ptr<Security>&& sec)
: Worker(id, std::move(sec))
 {
   // create a unique pointer
   cephCtx_ = std::make_unique<CephContext>(ctx);
 }


StoreWorker::StoreWorker(std::unique_ptr<CephContext>&& ctx,
                         const uint32_t id,
                         std::shared_ptr<Security>&& sec)
: Worker(id, std::move(sec)),
  cephCtx_(std::move(ctx))
{}


StoreWorker::~StoreWorker()
{

  // destroy the Ceph Context Object
  cephCtx_ = nullptr;
 
}


void 
StoreWorker::closeStoreWorker()
{
 
  // close the ceph context
  cephCtx_->closeCephContext();

  // release all the retrieved objects 
  // and enqueued tasks

  pendReads_.clear();


  std::deque<UpperRequest> cancelTasks;

  tasks_.dequeue(cancelTasks);
  
  // notify about an erorr
  for(auto& canTsk : cancelTasks)
  {
    Task tmpTask;
    tmpTask = std::move(canTsk.second);
    tmpTask.opStat_ = CommonCode::IOStatus::ERR_INTERNAL;

    // notify the server that there was an error
    canTsk.first.setValue(std::move(tmpTask));
  }

  // clear all tasks
  cancelTasks.clear();
  


  // once everything has been cleaned up
  // notify the caller
  
  {
    std::lock_guard<std::mutex> tmpLock(initLock_);
    init_ = false; // has been closed
    
    initCond_.notify_all();

  } // lock scope

}




bool 
StoreWorker::initialize()
{
  // initialize the ceph context
  const bool initCeph = cephCtx_->initAndConnect();

  if(!initCeph) // do not process further
  {
    return false;
  }

  // create a hash of the shared secret
  SHA256_CTX shaCtx;
  int initSha = SHA256_Init(&shaCtx);
  
  if(!initSha)
  {
    return false;
  }
  
  // compute the SHA-256 digest
  initSha = SHA256_Update(&shaCtx, (unsigned char*)WORKER_SHARED_SECRET, 
                          std::strlen(WORKER_SHARED_SECRET));

  if(!initSha)
  {
    return false;
  } 

  // compute the digest
  initSha = SHA256_Final(workerSecret, &shaCtx);
  
  if(!initSha)
  {
    return false; // cannot compute the digest
  }

  
  Worker::initDone(); // done initializing
 

  return true; // the worker has been initialized
}


Future<Task> 
StoreWorker::writeStoreObj(const Task& task)
{
  // this method shall be called from a Unix thread and
  // shall enqueue tasks for the worker.

  // create a promise for returing a future
  Promise<Task> prom;  

  auto res = prom.getFuture(); // for async computation
 
  // enqueue the promise and the task for the worker
  auto taskPair = std::make_pair(std::move(prom), task);

  // append a task
  tasks_.push(std::move(taskPair));


  // return the future   
  return std::move(res);
}

 

Future<Task> 
StoreWorker::readStoreObj(const Task& task)
{

  // this method shall be called from a Unix thread and
  // shall enqueue tasks for the worker.

  // create a promise for returing a future
  Promise<Task> prom;  

  auto res = prom.getFuture(); // for async computation
 
  // enqueue the promise and the task for the worker
  auto taskPair = std::make_pair(std::move(prom), task);

  // append a task
  tasks_.push(std::move(taskPair));


  // return the future   
  return std::move(res);

}


Future<Task> 
StoreWorker::deleteStoreObj(const Task& task)
{

  // this method shall be called from a Unix thread and
  // shall enqueue tasks for the worker.

  // create a promise for returing a future
  Promise<Task> prom;  

  auto res = prom.getFuture(); // for async computation
 
  // enqueue the promise and the task for the worker
  auto taskPair = std::make_pair(std::move(prom), task);

  // append a task
  tasks_.push(std::move(taskPair));


  // return the future   
  return std::move(res);   
}


Future<Task> 
StoreWorker::completeReadStoreObj(const Task& task)
{
  // this method shall be called from a Unix thread and
  // shall enqueue tasks for the worker.

  // create a promise for returing a future
  Promise<Task> prom;  

  auto res = prom.getFuture(); // for async computation
 
  // enqueue the promise and the task for the worker
  auto taskPair = std::make_pair(std::move(prom), task);

  // append a task
  tasks_.push(std::move(taskPair));


  // return the future   
  return std::move(res);

}


void
StoreWorker::processTasks()
{

  std::deque<UpperRequest> passedTasks;

  // process tasks from the concurretn queue
  while(!Worker::done_.load(std::memory_order_relaxed))
  {
    // wait for a few tasks
    tasks_.dequeue(passedTasks);
    
    // process any new tasks
    for(auto&& tmpTask : passedTasks)
    {

      // check the type of the task
      switch(tmpTask.second.opType_)
      {
        case CommonCode::IOOpCode::OP_READ: 
        {  // process read
         
           processReadOp(std::move(tmpTask));
           break; // one task has been completed
        }
      
        case CommonCode::IOOpCode::OP_WRITE:
        { // process write
        
          processWriteOp(std::move(tmpTask));

          break; // one task has been completed

        }
      
        case CommonCode::IOOpCode::OP_DELETE:
        { // process delete

          processDeleteOp(std::move(tmpTask));

          break; // one task has been completed
        }      

        default:
        {
          Worker::done_.store(true); // completed processing
        }
      } // switch

    } // for (new tasks)


    /* after issueing all new requests */
    /* process data operations */
    executeRadosOps();


  } // while

  /* closing procedures come here */
  closeStoreWorker();

}


void
StoreWorker::processReadOp(UpperRequest&& task)
{
  // first test if the operation is a pending one
  auto checkRead = pendReads_.find(task.second.tranID_);
  
  if(checkRead == pendReads_.end())
  { 
    // new READ operation (read new object)
    issueNewReadOp(std::move(task));
  }
  else
  { // pending READ operation
    if(checkRead->second.availableData())
    { // means no need to issue another read
      // since data is locally available
      handlePendingRead(checkRead, task);
    }
    else 
    { // need to bring data from the remote machine
      
    }
   
  }
}



void
StoreWorker::issueNewReadOp(UpperRequest&& task)
{

  // issue a READ request to the security module
  // and enqueue it 
  Task resTask(task.second); // copy the tast

  StoreWorker::WorkerContext readTmpCtx(
                               std::move(task),
                               std::move(
                                 secure_->checkPerm(
                                 resTask.path_, 
                             UserAuth(resTask.username_, 
                             reinterpret_cast<const char*>(workerSecret)),
                             CommonCode::IOOpCode::OP_READ)));


  // append to the list
  responses_.push_back(std::move(readTmpCtx));
}



void
StoreWorker::processWriteOp(UpperRequest&& task)
{
  // issue a WRITE request to the security module
  // and enqueue it 
  Task resTask(task.second); // copy the tast

  StoreWorker::WorkerContext writeTmpCtx(
                               std::move(task),
                               std::move(
                                 secure_->checkPerm(
                                 resTask.path_, 
                             UserAuth(resTask.username_, 
                             reinterpret_cast<const char*>(workerSecret)),
                             CommonCode::IOOpCode::OP_WRITE)));


  // appned to the list
  responses_.push_back(std::move(writeTmpCtx));
}


void
StoreWorker::processDeleteOp(UpperRequest&& task)
{
  // issue a DELETE request to the security module
  // and enqueue it 
  Task resTask(task.second); // copy the tast

  StoreWorker::WorkerContext delTmpCtx(
                               std::move(task),
                               std::move(
                                 secure_->checkPerm(
                                 resTask.path_, 
                             UserAuth(resTask.username_, 
                             reinterpret_cast<const char*>(workerSecret)),
                             CommonCode::IOOpCode::OP_DELETE)));


  // append the request to the list
  responses_.push_back(std::move(delTmpCtx));

}



void 
StoreWorker::handlePendingRead(
       std::map<uint32_t, StoreObj>::iterator& itr, 
       std::pair<folly::Promise<Task>, Task>& task)
{
 
  // check if the Task path mathes the unique ID
  assert(itr->second.getGlobalObjectId().compare(task.second.path_));
  
  // create the resulting task
  Task resTask(task.second);

  uint64_t dataRead = static_cast<uint64_t>(resTask.dataSize_);
  char* rawData = itr->second.getRawBytes(dataRead);
  assert(rawData); // ensure that there is data

  // write to the data to the shared memory
  // (use C-type casting since C++ complains)
  char* ptr = reinterpret_cast<char*>(resTask.dataAddr_);

  std::memcpy(ptr, rawData, dataRead); // write data to the memory
  
  // notify the user about written data
  resTask.dataSize_ = static_cast<uint32_t>(dataRead);
  
  // check if I can still read more data
  if(itr->second.storeObjComplete())
  { // done reading the entire object
    resTask.opStat_ = CommonCode::IOStatus::STAT_SUCCESS;
    // remove the iterator from the map
    pendReads_.erase(itr);
  }
  else
  { // need to read more data
    resTask.opStat_ = CommonCode::IOStatus::STAT_PARTIAL_READ;
  } 
  
  // a READ operation completed
  // notify the future
  task.first.setValue(std::move(resTask));

}



void
StoreWorker::executeRadosOps()
{
  auto opIter = responses_.begin();       // for erasing
  decltype(responses_.begin()) prevIter;  // an item
  
  while(opIter != responses_.end())
  {
    // check if the operation is completed
    if(opIter->secRes->isReady())
    { // handle the request and delete this item
     
      // prepare deleting the current item
      prevIter = opIter; 
      ++opIter;
      responses_.erase(prevIter);
    }// if

    
  }// while
  
}

} // namesapce singaistorageaipc
