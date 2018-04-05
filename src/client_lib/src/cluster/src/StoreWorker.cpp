// C++ std
#include <cassert>
#include <cstring>
// Facebook folly




// Project
#include "StoreWorker.h"



namespace singaistorageipc
{

using folly::Future;
using folly::Promise;

  
StoreWorker::StoreWorker(const CephContext& ctx, const unsigned int id, 
                std::shared_ptr<Security> sec)
: Worker(id, sec),
  cephCtx_(ctx)
 {}


StoreWorker::~StoreWorker()
{
  // close the ceph context
  cephCtx_.closeCephContext();

  // release all the retrieved objects 
  // and enqueued tasks

  pendReads_.clear();


  while(!tasks_.empty()) // since the worker is being destroyed,
  {                      // check for emptyness without a lock

    tasks_.pop();
  }

}


bool 
StoreWorker::initialize()
{
  // initialize the ceph context
  const bool res = cephCtx_.initAndConnect();

  if(res)
  {
    Worker::initDone(); // done initializing
  }

  return res;
}

void
StoreWorker::stop()
{
  // signal to finish
  Worker::done_.store(true);
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

  // process tasks from the concurretn queue
  while(!Worker::done_.load())
  {
    // wait for a task 
    auto tmpTask = std::move(tasks_.pop());

    // check the type of the task
    switch(tmpTask.second.opType_)
    {
      case Task::OpType::READ: 
      {  // process read
         
         processReadOp(tmpTask);
         break; // one task has been completed
      }
      
      case Task::OpType::WRITE:
      { // process write
        
        processWriteOp(tmpTask);

        break; // one task has been completed

      }
      
      case Task::OpType::DELETE:
      { // process delete

        processDeleteOp(tmpTask);

        break; // one task has been completed
      }      

     case Task::OpType::CLOSE:
     default:
     {
       done_.store(true); // completed processing
     }
   } // switch


  } // while

  /* closing procedures come here */


}


void
StoreWorker::processReadOp(
        std::pair<folly::Promise<Task>, const Task&>& task)
{
  // first test if the operation is a pending one
  auto checkRead = pendReads_.find(task.second.tranID_);
  
  if(checkRead == pendReads_.end())
  { 
    // new READ operation
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
StoreWorker::processWriteOp(
        std::pair<folly::Promise<Task>, const Task&>& task)
{
}


void
StoreWorker::processDeleteOp(
        std::pair<folly::Promise<Task>, const Task&>& task)
{
}





void 
StoreWorker::handlePendingRead(
       std::map<uint32_t, StoreObj>::iterator& itr, 
       std::pair<folly::Promise<Task>, const Task&>& task)
{
 
  // check if the Task path mathes the unique ID
  assert(itr->second.getGlobalObjectId().compare(task.second.path_));
  
  uint64_t dataRead = static_cast<uint64_t>(task.second.dataSize_);
  char* rawData = itr->second.getRawBytes(dataRead);
  assert(rawData); // ensure that there is data

  // write to the data to the shared memory
  // (use C-type casting since C++ complains)
  char* ptr = (char*) task.second.dataAddr_;

  std::memcpy(ptr, rawData, dataRead); // write data to the memory
  
  // notify the user about it
  Task tmp(task.second); // copy constructor
  tmp.dataSize_ = static_cast<uint32_t>(dataRead);
  
  // check if I can still read more data
  if(itr->second.storeObjComplete())
  { // done reading the entire object
    tmp.opCode_ = Task::OpCode::OP_SUCCESS;
    // remove the iterator from the map
    pendReads_.erase(itr);
  }
  else
  { // need to read more data
    tmp.opCode_ = Task::OpCode::OP_PARTIAL_READ;
  } 
  
  // a READ operation completed
  // notify the future
  task.first.setValue(std::move(tmp));

}

} // namesapce singaistorageaipc
