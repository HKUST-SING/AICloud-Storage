// C++ std


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

  pendTasks_.clear();


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





} // namesapce singaistorageaipc
