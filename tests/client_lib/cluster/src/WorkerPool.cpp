

// C++ std lib
#include <thread>
#include <cassert>
#include <cstdlib>
#include <ctime>
#include <stdexcept>


// project libs
#include "cluster/WorkerPool.h"
#include "cluster/CephContext.h"
#include "cluster/RemoteProtocol.h"


const constexpr unsigned int MAX_WORKERS = 48;
const constexpr unsigned int LEAVE_FREE  = 3;


namespace singaistorageipc
{

WorkerPool::WorkerPool(const unsigned int poolId,
                       const unsigned int poolSize)
: poolID_(static_cast<const uint32_t>(poolId)),
  active_(false)
{

  if(poolSize == 0)
  { // try to retrieve a number of concurrent ops
 
    const unsigned int availCores = std::thread::hardware_concurrency();
    if(availCores == 0)
    {
      // use the default value
      poolSize_ = WorkerPool::DEFAULT_POOL_SIZE;
    }
    else
    {
      // use the value
      if (availCores > 10)
      {
        poolSize_ = 10;
      }
      else
      {
        poolSize_ = (availCores > LEAVE_FREE) ? static_cast<uint32_t>(availCores - LEAVE_FREE) : 1;
      }
      
  
    }// retrieved number of cores
 
  }//if
  else
  {
    poolSize_ = (poolSize > MAX_WORKERS) ? static_cast<uint32_t>(MAX_WORKERS) : static_cast<uint32_t>(poolSize); // use the value
  }

  // preallocate memory
  workers_.resize(poolSize_);
  threads_.resize(poolSize_); // number of threads

}



WorkerPool::~WorkerPool()
{
  if(active_) // need to stop the pool?
  {
    stopPool();
  }

}


bool
WorkerPool::initialize(const char* cephFile,
                       const std::string& userName,
                       const std::string& clusterName,
                       const uint64_t cephFlags,
                       const char* protocolType,
                       std::shared_ptr<Security> sec
                      )
{

  // assume successful initialization
  active_ = true;

  uint32_t tmpID = 1; // worker ID

  for(auto& worker : workers_)
  {
   
    auto tmpPtr = createWorker(cephFile, userName, clusterName,
                               cephFlags, protocolType, sec,
                               tmpID);

    worker.swap(tmpPtr); // swap the managed objects
    
    ++tmpID; // update the id
  }

  // try to initialize all the workers
  bool success = true;

  for(const auto& worker : workers_ )
  {
    success = worker->initialize();
    if(!success)
    {
      break; // cannot initialize a worker
    }
 
  }

  if(!success)
  {
    stopPool();
  }
  else
  {
    std::srand(std::time(0)); // initialize random number generator

    for(uint32_t idx = 0; idx < poolSize_; ++idx)
    {
      // run a worker in a seprate thread
      std::thread runOp(&Worker::processTasks, workers_[idx].get());
      threads_[idx].swap(runOp);
    }
  }


  return success; // final result

}



void
WorkerPool::stopPool()
{
  if(!active_)
  {
    return; // nothing to stop
  }

  for(const auto& worker : workers_)
  {
    worker->stopWorker();
  }

  // wait all the threads to terminate
  for(auto& runThr : threads_)
  {
    if(runThr.joinable())
    {
      runThr.join(); // join the thread
    }
  }

  active_ = false; // the pool has been stopped  

}


std::unique_ptr<Worker>
WorkerPool::createWorker(const char* cephConf, 
                         const std::string& userName,
                         const std::string& clusterName,
                         const uint64_t cephFlags,
                         const char* protocolType,
                         std::shared_ptr<Security> sec,
                         const uint32_t workerID) const
{
  auto cephCtx = std::make_unique<CephContext>(cephConf, userName,
                                               clusterName, cephFlags);

  auto remProt = RemoteProtocol::createRemoteProtocol(protocolType);
  
  assert(cephCtx && remProt);

  return Worker::createRadosWorker("Default", std::move(cephCtx),
                                   std::move(remProt), workerID,
                                   sec);

}


uint32_t
WorkerPool::getSize() const noexcept
{
  return poolSize_;
}


uint32_t
WorkerPool::getPoolID() const noexcept
{
  return poolID_;
}


folly::Future<Task> 
WorkerPool::sendTask(Task task)
{

  if (task.workerID_ == 0 || task.workerID_ > poolSize_)
  {
    // pick a random worker
    auto workIdx = static_cast<const uint32_t>(std::rand()) % poolSize_;

    task.workerID_ = workIdx + 1;
    
    return workers_[workIdx]->readStoreObj(task);
  }
  else
  {
    return workers_[task.workerID_ - 1]->readStoreObj(task);
  }
}


folly::Future<Task>
WorkerPool::sendTask(Task task, const uint32_t workerID)
{

  if(workerID == 0 || workerID > poolSize_)
  {
    throw std::logic_error("WorkerPool::sendTask(Task task, const uint32_t workerID): workerID cannot be 0 or greater than poolSize_");
  }

  return workers_[workerID-1]->readStoreObj(task);

}


WorkerPool::BroadResult
WorkerPool::broadcastTask(Task task)
{

  BroadResult futures;

  // call on each of the worker to broadcast the task
  
  for(const auto& oneWork : workers_)
  {
    futures.push_back(std::move(oneWork->readStoreObj(task)));
  }


  return futures;

}


std::shared_ptr<WorkerPool>
WorkerPool::createWorkerPool(const char* poolType,
                             const unsigned poolID,
                             const unsigned poolSize)
{

  return std::make_shared<WorkerPool>(poolID, poolSize);

}

} // namespace singaistorageipc
