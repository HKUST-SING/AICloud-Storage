// C++ std


// Facebook folly




// Project
#include "store_worker.h"



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
} // namesapce singaistorageaipc
