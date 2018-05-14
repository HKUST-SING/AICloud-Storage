// C++ std lib
#include <utility>
#include <cassert>
#include <limits>

//Google lib
#include <glog/logging.h>

// Project lib
#include "cluster/CephContext.h"

#define SING_RADOS_SECONDS_TO_SLEEP 5



// sizes of the two intergal types for making sure that no overflow
// occurs wile using librados::bufferlist
static constexpr const bool POSSIBLE_OVERFLOW = (sizeof(size_t) > sizeof(unsigned int));


// do precasting for better performance
// (if sizeof(size_t) > sizeof(unsigned int), get maximum value in size_t)
static constexpr const unsigned int MAX_RADOS_OP_SIZE = (std::numeric_limits<unsigned int>::max() >> 1);

static constexpr const size_t MAX_BUFFER_SIZE = static_cast<const size_t>(MAX_RADOS_OP_SIZE);


namespace singaistorageipc
{

using std::string;


// converts size_t to such a value that librados::bufferlist
// never overflows
static size_t maximumDataOperationSize(const size_t dataSize)
{


  // if size_t can store larger values than 
  // unsigned int, need to cap the value
  if(POSSIBLE_OVERFLOW && dataSize > MAX_BUFFER_SIZE) 
  {
    return MAX_BUFFER_SIZE;
  }//if
 
 
  return dataSize; // no need to cap the value

}

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

  // delete the handler
  if(opHandler_)
  {
    delete opHandler_;
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
    LOG(WARNING) << "Cannot initialize the cluster handle";
    return false; // cannot initialize the handle
  }


  // read a Ceph configuration file to configure the cluster handle
  ret = cluster_.conf_read_file(confFile_);
  
  if(ret < 0)
  {
    LOG(WARNING) << "Cannot read Rados configuration file";
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
  opHandler_->startRadosOpHandler();
 

  // mark the Ceph context as initliazed
  init_ = true;

  DLOG(INFO) << "Successfully started a ceph context";

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
  done_(true),
  cluster_(rados)
{
  // make sure non-null
  assert(rados);
}




size_t
CephContext::readObject(const std::string& oid,
                        const std::string& poolName,
                        const size_t readBytes,
                        const uint64_t offset,
                        void* userCtx)
{

  if(!init_)
  {
    return 0; // not connected or already closed
  }

  // already exists?
  if(!checkPoolExists(poolName))
  { // if the pool has not been accessed before
    // try to connect to it
    auto addRes = addRadosContext(poolName);
    
    if(!addRes)
    { // something went wrong
      LOG(ERROR) << "Cannot add a new Rados Context (cannot connect)";
 
      return 0;
    }
 
  } //if

  // have an active IO Context
  // issue a read operation
  auto& ioRadCtx = getPoolRadosContext(poolName);

  // try to read
  auto readOpSize = opHandler_->readRadosObject(&ioRadCtx, oid, readBytes, 
                                                offset, userCtx);

  if(!readOpSize)
  {
    LOG(WARNING) << "Cannot read data from handler";
  }
  
  
  return readOpSize;
}



size_t
CephContext::writeObject(const std::string& oid, 
                         const std::string& poolName,
                         const char* buffer, const size_t writeBytes,
                         const uint64_t offset, const bool append,
                         void* userCtx)
{

  if(!init_) // either not connected yet or has already been closed
  {
    return 0;
  }

  
  // check if the data pool already exists
  if(!checkPoolExists(poolName))
  { // if the pool has not been accessed before
    // try to connect to it
    auto addRes = addRadosContext(poolName);
    
    if(!addRes)
    { // something went wrong
      LOG(ERROR) << "Cannot add a new Cpeh Context " 
                 << "(cannot connect to cluster).";
 
      return 0;
    }
 
  } //if

  // have an active IO Context
  // issue a write operation
  auto& ioRadCtx = getPoolRadosContext(poolName);

  // try to write
  auto writeOpSize = opHandler_->writeRadosObject(&ioRadCtx, oid, 
                                                 buffer, writeBytes, 
                                                 offset, append,
                                                 userCtx);

  if(!writeOpSize)
  {
    LOG(WARNING) << "CephContext cannot write data";
  }

  return writeOpSize;



}


size_t
CephContext::writeObject(const std::string& oid, 
                         const std::string& poolName,
                         const librados::bufferlist& buffer, 
                         const size_t writeBytes,
                         const uint64_t offset, const bool append,
                         void* userCtx)
{

  if(!init_) // either not connected yet or has already been closed
  {
    return 0;
  }

  
  // check if the data pool already exists
  if(!checkPoolExists(poolName))
  { // if the pool has not been accessed before
    // try to connect to it
    auto addRes = addRadosContext(poolName);
    
    if(!addRes)
    { // something went wrong
      LOG(ERROR) << "Cannot connect to the Ceph cluster";
 
      return 0;
    }
 
  } //if

  // have an active IO Context
  // issue a write operation
  auto& ioRadCtx = getPoolRadosContext(poolName);

  // try to write
  auto writeOpSize = opHandler_->writeRadosObject(&ioRadCtx, oid, 
                                                 buffer, writeBytes, 
                                                 offset, append,
                                                 userCtx);

  if(!writeOpSize)
  {
    LOG(WARNING) << "Ceph handler cannot write data";
  }

  return writeOpSize;



}





CephContext::PollSize
CephContext::getCompletedOps(std::deque<CephContext::RadosOpCtx*>& ops)
{

  if(!opHandler_) // not connected yet
  {
    return 0;
  }

  auto oldSize = ops.size(); // get the current number of ops stored

  // try to retrieve some new ops
  opHandler_->pollAsyncIOs(ops);

  return (ops.size() - oldSize);
}



CephContext::RadosOpHandler::~RadosOpHandler()
{

  if(!done_.load(std::memory_order_acquire))
  { // need to stop the handler
    stopRadosOpHandler();
  }

  // Try to dequeue all the remaining contexts and destroy
  // them
  clearPendingAIOs();
}



void
CephContext::RadosOpHandler::clearPendingAIOs()
{

  // We can test for length since no more active operations 
  if(!asyncOps_.empty())
  {  
    std::deque<CephContext::RadosOpCtx*> completedIOs;
    asyncOps_.dequeue(completedIOs); 

    // go over all the contexts and delete them
    for(auto& ctx : completedIOs)
    {
      ctx->userCtx = nullptr;
      assert(ctx->release());
    }// for
 
    completedIOs.clear(); // done deeleting
 }//if

}


bool
CephContext::RadosOpHandler::startRadosOpHandler()
{

  // start the handler
  done_.store(false, std::memory_order_release);

  return true;
}


void
CephContext::RadosOpHandler::stopRadosOpHandler()
{

  // notify everyone that it has been completed
  done_.store(true, std::memory_order_release);
  const unsigned int pendOps = activeIOs_.load(std::memory_order_acquire);

  DLOG(INFO) << "RadosOpHandler:stopRadosOpHandler";

  if(pendOps > 0)
  {
    // wait for all operations to complete 
    std::unique_lock<std::mutex> tmpLock(termLock_);
    while(activeIOs_.load(std::memory_order_acquire) > 0)
    {
      termCond_.wait(tmpLock);
    }    

  } // lock scope
  
}



void
CephContext::RadosOpHandler::pollAsyncIOs(
             std::deque<CephContext::RadosOpCtx*>& completedAIOs)
{
 
  // just use the concurrent queue deque method
  asyncOps_.dequeue(completedAIOs);

}



void
CephContext::RadosOpHandler::radosAsyncCompletionCallback(
                               librados::completion_t cb,
                               void* args)
{
  // we cast void to the passed context
  CephContext::RadosOpHandler::CmplCtx* opCtx =\
      static_cast<CephContext::RadosOpHandler::CmplCtx*>(args);

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
  
  // copy the rados context
  CephContext::RadosOpCtx* retCntx = opCtx->radosCtx;
  opCtx->radosCtx = nullptr;  

  // delete the async operation and its context
  opCtx->release();

  // if the operation is write
  // clear the buffer
  if(retCntx->opType == CommonCode::IOOpCode::OP_WRITE)
  {
    // clear the data buffer (no need anymore)
    retCntx->opData.clear();
  }

  
  // finally enqueue the result
  asyncOps_.push(retCntx);

  // mark the operation as completed one
  const unsigned int leftVal =\
                 activeIOs_.fetch_sub(1, std::memory_order_acquire);

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


// A helper function to make it possible to use a callback on a method.
void 
CephContext::RadosOpHandler::aioRadosCallbackFunc(librados::completion_t cb,
                                                  void* args)
{
 
  const CephContext::RadosOpHandler::CmplCtx* tmpCtx =\
          static_cast<const CephContext::RadosOpHandler::CmplCtx*>(args); // cast the pointer (Context) 

  // call the method to handle the operation 
  tmpCtx->objPtr->radosAsyncCompletionCallback(cb, args);

}



size_t
CephContext::RadosOpHandler::readRadosObject(librados::IoCtx* ioCtx,
                             const std::string& objId,
                             const size_t readBytes,
                             const uint64_t offset,
                             void* userCtx)
{

  // The following code is taken from the Ceph librados tutorial
  /****************** Ceph Librados Tutorial ******************/
  // Create callback context

  CephContext::RadosOpHandler::CmplCtx* args = CephContext::RadosOpHandler::CmplCtx::createHandlerCompletionContext();

  // Create I/O Completion.
  
  librados::AioCompletion* readCompletion =\
            librados::Rados::aio_create_completion(\
              static_cast<void*>(args), 
              CephContext::RadosOpHandler::aioRadosCallbackFunc, 
              nullptr);


  // create an operation context
  CephContext::RadosOpCtx* opCtx = CephContext::RadosOpCtx::createRadosOpCtx();

  // initialize the Completion context
  args->objPtr   = this; // this object
  args->ioCmpl   = readCompletion;
  args->radosCtx = opCtx;

  // proivde opearation context
  opCtx->opType    = CommonCode::IOOpCode::OP_READ;
  opCtx->opStatus  = CommonCode::IOStatus::ERR_INTERNAL;
  opCtx->userCtx   = userCtx; // passed user context


  const size_t canReadAtOnce = maximumDataOperationSize(readBytes);

  const int ret = ioCtx->aio_read(objId, readCompletion, 
                                  &(opCtx->opData), canReadAtOnce,
                                  offset);


  if(ret < 0) // failure occurred
  {
    // log it first;
    LOG(WARNING) << "ioCtx->aio_read returned < 0";
    
    // delete all allocated fields
    args->release();

    return 0; // notify that cannot read 
  }


  DLOG(INFO) << "RadosOpHandler::readRadosObject: has read "
             << canReadAtOnce << " bytes";

  return canReadAtOnce; // return number of bytes to be read  
}


size_t
CephContext::RadosOpHandler::writeRadosObject(librados::IoCtx* ioCtx,
                             const std::string& objId,
                             const char* rawData,
                             const size_t writeBytes,
                             const uint64_t offset,
                             const bool appendData,
                             void* userCtx)
{
   
   // get the maximum number of bytes that can be written at once
  size_t canWriteAtOnce = maximumDataOperationSize(writeBytes);
  
  if(canWriteAtOnce == 0)
  {
    // log it before returning
    LOG(WARNING) << "maximumDataOperationSize == 0";
    return 0;
  }



    // The following code is taken from the Ceph librados tutorial
  /****************** Ceph Librados Tutorial ******************/
  // Create callback context

  CephContext::RadosOpHandler::CmplCtx* args = CephContext::RadosOpHandler::CmplCtx::createHandlerCompletionContext();

  // Create I/O Completion.
  
  librados::AioCompletion* writeCompletion =\
            librados::Rados::aio_create_completion(
              static_cast<void*>(args), nullptr, 
              CephContext::RadosOpHandler::aioRadosCallbackFunc);



  // create an operation context
  CephContext::RadosOpCtx* opCtx = CephContext::RadosOpCtx::createRadosOpCtx();

  // initialize the Completion context
  args->objPtr  = this; // this object
  args->ioCmpl  = writeCompletion;
  args->radosCtx = opCtx;

  // proivde opearation context
  opCtx->opType    = CommonCode::IOOpCode::OP_WRITE;
  opCtx->opStatus  = CommonCode::IOStatus::ERR_INTERNAL;
  opCtx->userCtx   = userCtx; // passed user context

 
  // copy the data to a librados::bufferlist
  opCtx->opData.append(rawData, static_cast<const unsigned int>(canWriteAtOnce));


  int ret = 0; // Rados  op result flag
 
  while(canWriteAtOnce) // as long as there is some data to write
  {

    if(appendData)
    { // try to append data

      ret = ioCtx->aio_append(objId, writeCompletion, 
                                    opCtx->opData, canWriteAtOnce);
    }
    else
    { // write at the provided offset
      ret = ioCtx->aio_write(objId, writeCompletion, 
                                    opCtx->opData, canWriteAtOnce,
                                    offset);
    }

    if(ret == -E2BIG) // menas it's too much data (cannot write more)
    {
      // half the value
      canWriteAtOnce >>= 1; // reduce the write size
    }
    else
    {
      // break
      break; // don't try to write again
    }

  }// while
 
 

  if(ret < 0) // failure occurred
  {
    // check if the size failure occurred
    

    // log it first;
    LOG(ERROR) << "RadosOpHandler: cannot write after many attempts";
    
    // delete all allocated fields
    args->release();

    return 0; // notify that cannot read 
  }

  DLOG(INFO) << "RadosOpHandler::writeRadosObject: has written "
             << canWriteAtOnce << " bytes";

  return canWriteAtOnce; // return number of bytes to be read
}



size_t
CephContext::RadosOpHandler::writeRadosObject(librados::IoCtx* ioCtx,
                             const std::string& objId,
                             const librados::bufferlist& buffer,
                             const size_t writeBytes,
                             const uint64_t offset,
                             const bool appendData,
                             void* userCtx)
{

  // get the maximum number of bytes that can be written at once
  size_t canWriteAtOnce = maximumDataOperationSize(writeBytes);
  
  if(canWriteAtOnce == 0)
  {
    // log it before returning
    LOG(WARNING) << "maximumDataOperationSize == 0";
    return 0;
  }


    // The following code is taken from the Ceph librados tutorial
  /****************** Ceph Librados Tutorial ******************/
  // Create callback context

  CephContext::RadosOpHandler::CmplCtx* args = CephContext::RadosOpHandler::CmplCtx::createHandlerCompletionContext();

  // Create I/O Completion.
  
  librados::AioCompletion* writeCompletion =\
            librados::Rados::aio_create_completion(
              static_cast<void*>(args), nullptr, 
              CephContext::RadosOpHandler::aioRadosCallbackFunc);

  // create an operation context
  CephContext::RadosOpCtx* opCtx = CephContext::RadosOpCtx::createRadosOpCtx();

  // initialize the Completion context
  args->objPtr  = this; // this object
  args->ioCmpl  = writeCompletion;
  args->radosCtx = opCtx;

  // proivde opearation context
  opCtx->opType    = CommonCode::IOOpCode::OP_WRITE;
  opCtx->opStatus  = CommonCode::IOStatus::ERR_INTERNAL;
  opCtx->userCtx   = userCtx; // passed user context



  int ret = 0; // Rados  op result flag

  while(canWriteAtOnce)
  {
  
    if(appendData)
    { // try to append data

      ret = ioCtx->aio_append(objId, writeCompletion, 
                                     buffer, canWriteAtOnce);
    }
    else
    { // write at the provided offset
      ret = ioCtx->aio_write(objId, writeCompletion, 
                                    buffer, canWriteAtOnce,
                                    offset);
    }

    if(ret == -E2BIG)
    {
      canWriteAtOnce >>= 1; // try to reduce the write size
    }
    else
    {
      break; // don't care about other cases   
    }

  }//while


  if(ret < 0) // failure occurred
  {
    // log it first;
    LOG(ERROR) << "RadosOpHandler: cannot write after multiple attempts";
    
    // delete all allocated fields
    args->radosCtx->userCtx = nullptr;
    args->release();

    return 0; // notify that cannot write 
  }

  DLOG(INFO) << "RadosOpHandler::writeRadosObject: has written " 
             << canWriteAtOnce << " bytes";

  return canWriteAtOnce; // return number of bytes to be read
}


} // namesapce singaistorageipc
