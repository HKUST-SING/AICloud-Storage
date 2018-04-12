// C++ std lib
#include <utility>
#include <cassert>
#include <chrono>
#include <thread>


// Project lib
#include "cluster/CephContext.h"

#define SING_RADOS_SECONDS_TO_SLEEP 5

namespace singaistorageipc
{

using std::string;


CephContext::CephContext(const char* conf, const string& userName, 
                         const string& clusterName,
                         const uint64_t flags)
: clusterName_(clusterName), 
  accessName_(userName),
  confFile_(conf),
  accessFlags_(flags),
  init_(false),
  opHandler_(nullptr)
{}


CephContext::CephContext(const char* conf, string&& userName, 
                         string&& clusterName,
                         const uint64_t flags)
: clusterName_(std::move(clusterName)), 
  accessName_(std::move(userName)),
  confFile_(conf),
  accessFlags_(flags),
  init_(false),
  opHandler_(nullptr)
{}


CephContext::CephContext(const CephContext& other)
: clusterName_(other.clusterName_),
  accessName_(other.accessName_),
  confFile_(other.confFile_),
  accessFlags_(other.accessFlags_),
  init_(false),
  opHandler_(nullptr)
{}



CephContext::~CephContext()
{
  if(init_) // check if the context needs to be closed
  {
    closeCephContext();
    init_ = false;
  }

  // reset the configuration file
  confFile_ = nullptr; 

}


bool
CephContext::initAndConnect()
{

  if(init_) // already initialized
  {
   return true;
  }

  // try first to initialize the rados handle
  int ret = cluster_.init2(accessName_.c_str(), clusterName_.c_str(),
                           accessFlags_);



  // if cannot initialize the handle, return false
  if(ret < 0)
  {
    return false; // cannot initialize the handle
  }


  // read a Ceph configuration file to configure the cluster handle
  ret = cluster_.conf_read_file(confFile_);
  
  if(ret < 0)
  {
    cluster_.shutdown(); // close the handle first
    return false;
  }


  // try to connect to the cluster
  ret = cluster_.connect();
  
  if(ret < 0)
  {
    cluster_.shutdown(); // close the cluster
    return false;
  }
 
  // initialize the rados op handler
  opHandler_ = new CephContext::RadosOpHandler(&cluster_);
 

  // mark the Ceph context as initliazed
  init_ = true;

  return true; // successfully initialized the cluster handle
}



void
CephContext::closeCephContext()
{
  if(!init_) // not initialized
  {
    return;
  }


  // close/terminate the Rados Op Handler
  opHandler_->stopRadosOpHandler(); // wait/terminate current IOs 
  // delete the handler
  delete opHandler_;
  opHandler_ = nullptr;


  // first close all rados context instances
  for(const auto& value : ioOps_)
  {
    closeRadosContext(value.first);
  }

  // now clear the map
  ioOps_.clear();


  // close the handle to the cluster
  cluster_.shutdown();  


  // set as closed/non-initialized
  init_ = false;

}


bool
CephContext::connectRadosContext(const string& poolName)
{

  // check if connected
  if(!init_)
  {
    return false;
  }


  //create a librados io context
  librados::IoCtx tmpCtx;
  
  int ret = cluster_.ioctx_create(poolName.c_str(), tmpCtx);

  if(ret < 0)
  {
   // some error
   return false;
  }


  // add the newly created io context to the map
  auto tmpPair = std::make_pair<string, librados::IoCtx>(string(poolName), std::move(tmpCtx));
  
  auto res = ioOps_.insert(std::move(tmpPair));


  // if cannot insert the pool, close the context
  if(!res.second)
  {
    // cannot insert
    tmpCtx.close();
    return false;
  }
  

  return true; // all cases passed (successfully inserted)
 
}



bool
CephContext::closeRadosContext(const string& poolName)
{

  auto itr = ioOps_.find(poolName); // get reference to a pool

  itr->second.close(); // close the io context  

  return true;
}


CephContext::RadosOpHandler::RadosOpHandler(librados::Rados* rados) noexcept
: activeIOs_(0),
  done_(false),
  cluster_(rados)
{
  // make sure non-null
  assert(rados);
}


CephContext::RadosOpHandler::~RadosOpHandler()
{}


void
CephContext::RadosOpHandler::stopRadosOpHandler()
{

  // notify everyone that it has been completed
  done_.store(true, std::memory_order_release);
  const unsigned int pendOps = activeIOs_.load(std::memory_order_relaxed);

  if(!pendOps)
  {
    return; // no active IO operations (safe to close)
  }

  // wait for all operations to complete
  {
    std::unique_lock<std::mutex> tmpLock(termLock_);
    while(activeIOs_.load(std::memory_order_acquire))
    {
      termCond_.wait(tmpLock);
    }    


  } // lock scope
  

  // done waiting for IO completions
  // release all the memory allocated to the 
  // currently enqueued operations
  std::deque<CephContext::RadosOpCtx*> completedIOs;
  asyncOps_.dequeue(completedIOs); 

  // go over all the contexts and delete them
  for(auto& ctx : completedIOs)
  {
    ctx->userCtx = nullptr;
    assert(ctx->release());
  }
 
  completedIOs.clear(); // done deeleting

}



void
CephContext::RadosOpHandler::pollAsyncIOs(
             std::deque<CephContext::RadosOpCtx*>& completedAIOs)
{
 
  // just use the concurrent queue deque method
  asyncOps_.dequeue(completedAIOs);

}


unsigned int
CephContext::RadosOpHandler::readRadosObject(librados::IoCtx* ioCtx,
                             const std::string& objId,
                             const unsigned int readBytes,
                             const uint64_t offset,
                             void* userCtx)
{

  return 0;
}


unsigned int
CephContext::RadosOpHandler::writeRadosObject(librados::IoCtx* ioCtx,
                             const std::string& objId,
                             const char* rawData,
                             const unsigned int writeBytes,
                             const uint64_t offset,
                             const bool appendData,
                             void* userCtx)
{

  return 0;
}



void
CephContext::RadosOpHandler::radosAsyncCompletionCallback(
                               librados::completion_t cb,
                               void* args)
{
  // we cast void to the passed context
  CephContext::RadosOpHandler::CmplCtx* opCtx =\
      reinterpret_cast<CephContext::RadosOpHandler::CmplCtx*>(args);

  assert(opCtx != nullptr); // make sure no failure

  const int retVal = opCtx->ioCmpl->get_return_value();
  if(retVal < 0)
  { // some Rados error occurred
    opCtx->radosCtx->opStatus = CommonCode::IOStatus::ERR_INTERNAL;
  }
  else  // succesful Rados operation
  {
    opCtx->radosCtx->opStatus = CommonCode::IOStatus::STAT_SUCCESS;
  } 
  
  // coppy the rados context
  CephContext::RadosOpCtx* retCntx = opCtx->radosCtx;
  opCtx->radosCtx = nullptr;  


  // delete the async operation and its context
  opCtx->ioCmpl->release(); // deletes itself
  delete opCtx;
   

  
  // finally enqueue the result
  asyncOps_.push(retCntx);

  // mark the operation as completed one
  const unsigned int leftVal =\
                 activeIOs_.fetch_sub(1, std::memory_order_release);

  if(leftVal == 1) // no more operations left (this one is the last one)
  {
    // check whether the handler is being terminated
    const bool needToFinish = done_.load(std::memory_order_acquire);
    if(needToFinish)
    { 
      // notify the only waiting thread (no need to hold the lock)
      termCond_.notify_one();
      
    } // if needToFinish
  }// if leftVal == 0
}


} // namesapce singaistorageipc
