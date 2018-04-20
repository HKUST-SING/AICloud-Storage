// C++ std
#include <cassert>
#include <cstring>
#include <limits>

// Facebook folly



// Project
#include "StoreWorker.h"
#include "remote/Authentication.h"

#define WORKER_SHARED_SECRET "workersharedsecretpassword"

// maximum Rados IO Operation. Rados cannot read/write data greater 
// than (unsigned int max)/2. See ceph::bufferlist and 
// ceph::libraods for details.
static constexpr const uint64_t MAX_IO_SIZE = static_cast<const uint64_t>((std::numeric_limits<unsigned int>::max() >> 1));


using folly::Future;
using folly::Promise;

namespace singaistorageipc
{



StoreWorker::OpContext::OpContext()
: prot{nullptr},
  object(),
  tranID(0),
  backID(0),
  active(true)
{
  
}


StoreWorker::OpContext::OpContext(std::shared_ptr<RemoteProtocol::ProtocolHandler> ph, DataObject&& ob)
: prot(ph),
  object(std::move(ob)),
  tranID(0),
  backID(0),
  active(true)
{}


StoreWorker::OpContext::OpContext(struct StoreWorker::OpContext&& other)
: prot(std::move(other.prot)),
  object(std::move(other.object)),
  tranID(other.tranID),
  backID(other.backID),
  pendData(std::move(other.pendData)),
  pendRequests(std::move(other.pendRequests)),
  active(other.active)
{}
 


StoreWorker::OpContext::~OpContext()
{

  if(active)
  {
    closeContext();
  }

}
 

bool
StoreWorker::OpContext::insertReadBuffer(librados::bufferlist&& buffer,
                                         const uint32_t id)
{

  if(!active)
  {
    return false;
  }

  if(id == backID)
  {
    ReadObject* reader = object.getReadObject(false);
    assert(reader);

    bool res = reader->appendBuffer(std::move(buffer));  

    ++backID; // ensure a contiguous append
    while(!pendData.empty() && pendData.front().first == backID)
    {
      res = (res && reader->appendBuffer(std::move(pendData.front().second)));
      // remove the head
      pendData.pop_front();
      ++backID;
    }
  
    return res; // result of appending all data

  }
  else
  {
    // insert the buffer somewhere in the list
    if(pendData.empty() || pendData.back().first < id)
    {
      pendData.push_back(std::move(std::pair<uint32_t, librados::bufferlist>(id, std::move(buffer))));

      return true;
    }
    else
    { // insert at the proper position (we assume there will be no more
      //                                 (2^32 operations)

     for(auto itrVal = pendData.begin(); itrVal != pendData.end(); ++itrVal)
     {
       if(itrVal->first > id)
       {
         pendData.insert(itrVal, std::move(std::pair<uint32_t, librados::bufferlist>(id, std::move(buffer))));
         return true;
       }
       
     }//for
 
    }//else
  
  }//else


  return false;
}


bool
StoreWorker::OpContext::appendRequest(StoreWorker::UpperRequest&& req)
{
  if(active)
  {
    pendRequests.push_back(std::move(req));

    return true;
  }

  return false;
}

  
void
StoreWorker::OpContext::closeContext()
{

  if(!active)
  {
    return;
  }

  if(prot)
  {
    prot.reset(); // just destroy it
  }

  pendData.clear();


  if(object.validUserContext())
  {
    Task resTask(object.getTask());
    resTask.opStat_ = CommonCode::IOStatus::ERR_INTERNAL;
    object.setResponse(std::move(resTask));
  }
  
  for(auto& errTask : pendRequests)
  {
    Task resTask(std::move(errTask.second));
    resTask.opStat_   = CommonCode::IOStatus::ERR_INTERNAL;
    errTask.first.setValue(std::move(resTask));
  }

  pendRequests.clear();

  active = false;
}

StoreWorker::StoreWorker(std::unique_ptr<CephContext>&&    ctx,
                         std::unique_ptr<RemoteProtocol>&& prot,
                         const uint32_t id,
                         std::shared_ptr<Security> sec)
: Worker(id, sec),
  cephCtx_(ctx.release()),
  remProt_(prot.release())
{
}


StoreWorker::~StoreWorker()
{

  // destroy the Ceph Context Object and the Protocol
  delete cephCtx_;
  delete remProt_;
  
 
}


void 
StoreWorker::closeStoreWorker()
{ 

   // stop the protocol
  remProt_->closeProtocol();

  // close the ceph context
  cephCtx_->closeCephContext();

  // release all the retrieved objects 
  // and enqueued tasks

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

    if(passedTasks.empty())
    { // check other values
      if(activeOps_.empty() && responses_.empty() && pendTasksEmpty())
      { // block and wait for an operation
        StoreWorker::UpperRequest blockReq(std::move(tasks_.pop()));
        passedTasks.push_back(std::move(blockReq));
      }
      else
      { // 1: try to complete any active ops;
        // 2: try to check for any remote respones;
        // 3: try to issue a new task. 
        
        handleActiveOps();
        handleRemoteResponses();
        handlePendingTasks();


        continue; // loop again
      }//else

    }//if passedTasks.empty()

    
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
          Worker::done_.store(true, std::memory_order_release); 
                                        // completed processing
        }
      } // switch

    } // for (new tasks)
   
    passedTasks.clear();

    /* after issueing all new requests */
    /* process data operations */
    handleActiveOps();
    handleRemoteResponses();
    handlePendingTasks();


  } // while

  /* closing procedures come here */
  closeStoreWorker();

}


bool
StoreWorker::pendTasksEmpty() const
{
  if(pendTasks_.empty())
  {
    return true;
  }

  for(const auto& task : pendTasks_)
  {
    if(!task.second.empty())
    {
      return false;
    }
  }

  // no pending operations
  return true;

}



void
StoreWorker::handleRemoteResponses()
{}

void
StoreWorker::handlePendingTasks()
{}


void
StoreWorker::handleActiveOps()
{

  // poll the Rados contet for any completed messages
  std::deque<CephContext::RadosOpCtx*> compOps;

  if(cephCtx_->getCompletedOps(compOps) == 0)
  {
    return; // nothing has completed
  }
  else
  { // process the completed operations

    for(auto opCtx : compOps)
    {
      switch(opCtx->opType)
      {
        case OpCode::OP_WRITE:
        {
          // completed Rados write op
          processCompletedRadosWrite(opCtx);
          opCtx->userCtx = nullptr;
          opCtx->release(); // delete it
          break;
        } 
        case OpCode::OP_READ:
        {
          // completed Rados read op
          processCompletedRadosRead(opCtx);
          opCtx->userCtx = nullptr;
          opCtx->release(); // delete it
          break;
        }
        default:
        {
          opCtx->userCtx = nullptr;
          opCtx->release();
          int a = 5; // log this case
          assert(false); // fail in this case
        }

      } // switch
    }//for

    // clear the queue
    compOps.clear();
  }
}


void
StoreWorker::processCompletedRadosWrite(CephContext::RadosOpCtx* const opCtx)
{
  assert(opCtx);
  assert(opCtx->userCtx); // must not be nullptr

  StoreWorker::UserCtx* const procCtx = reinterpret_cast<StoreWorker::UserCtx* const>(opCtx->userCtx);

  assert(procCtx);

  //find an active write operation
  auto writeIter = activeOps_.find(procCtx->path);
  const uint32_t writeOpSize = procCtx->opID;
  delete procCtx; // don't need anymore  

  if(writeIter == activeOps_.end())
  {
    // log this as an error
    int a = 5;
    return;
  }

  // found an active write operation
  auto& opIter = writeIter->second;

  if(opCtx->opStatus != Status::STAT_SUCCESS)
  { // terminate the operation
    // no need to send anything on failed write (protocol)

    // WRITE operation must have at least one active
    // user request
    assert(opIter.object.validUserContext()); 

    Task resTask(opIter.object.getTask());
    resTask.workerID_ = Worker::id_;
    resTask.opStat_   = Status::ERR_INTERNAL;
    
    // notify the user
    assert(opIter.object.setResponse(std::move(resTask)));

    // close the context since there is no more operations
    opIter.closeContext();
    activeOps_.erase(writeIter); // completed op
    
  }// if != STAT_SUCCESS
  else
  { // successfully completed an operation
    // process the request
    // check if finished reading
    opIter.totalOpSize -= static_cast<uint64_t>(writeOpSize);
    if(opIter.prot->doneWriting() && opIter.totalOpSize == 0)
    {
      // completed writing 
      // send a response to the auth server
      sendWriteConfirmation(writeIter);
    }
    else
    {
      if(opIter.object.isComplete())
      {
        // get a write operation and send PARTIAL WRITE complete
        Task resTask(opIter.object.getTask());
        resTask.workerID_ = Worker::id_;
        resTask.opStat_ = Status::STAT_SUCCESS;

        opIter.object.setResponse(std::move(resTask));

        // if possible, update the write object
        if(opIter.pendRequests.empty())
        {
          return; // wait for a new WRITE operation
        }
        
        // can update the context
        updateWriteOpContext(writeIter);

      }//means need to issue another write operation
       // with the same context
       issueRadosWrite(writeIter);
      
    }// else
    
  }//else
}



void
StoreWorker::updateWriteOpContext(StoreWorker::OpItr& itr)
{

  assert(!itr->second.pendRequests.empty());
  
  auto& opIter = itr->second;

  // try to merge as many writes as possible into
  // one write
  WriteObject* opObj = new WriteObject(std::move(opIter.pendRequests.front()));
  opIter.pendRequests.pop_front(); // remove the random value
  
  // now append the write to the bufferlist
  const auto& task = opObj->getTask();
  
  // read data from the shared memory into the buffer
  const char* rawData = reinterpret_cast<const char*>(task.dataAddr_);
  assert(rawData);
  assert(opObj->appendBuffer(rawData, static_cast<const uint64_t>(task.dataSize_)));

  // also try to merge as many write ops as possible into
  // one write op
  bool appendMore = true;
  uint64_t avBuffer = opObj->availableBuffer();
  uint64_t needBuffer;
  auto& possibleTask = opIter.pendRequests.front().second;
  
  do
  {
    needBuffer = static_cast<uint64_t>(possibleTask.dataSize_);  
    if(needBuffer <= avBuffer)
    {
     // can merge two operations
     rawData = reinterpret_cast<const char*>(possibleTask.dataAddr_);
     assert(opObj->appendBuffer(rawData, needBuffer));
     avBuffer = opObj->availableBuffer();

     // issue merge signal
     Task resTask(std::move(possibleTask));
     resTask.opStat_ = Status::STAT_PARTIAL_WRITE;
     opIter.pendRequests.front().first.setValue(std::move(resTask));
     
     // done with this operation
     opIter.pendRequests.pop_front();
    }
    else
    {
      appendMore = false;
    }   

  } while(appendMore && !(opIter.pendRequests.empty()));

   opIter.object.setWriteObject(opObj);

  // merge as many write operations as possible
}


void
StoreWorker::issueRadosWrite(StoreWorker::OpItr& iter)
{

  assert(iter->second.object.validUserContext());

  auto& opIter = iter->second;

  WriteObject* objWrite = opIter.object.getWriteObject();
  assert(objWrite);

  // must always be a valid context
  StoreWorker::UserCtx* const procCtx = new StoreWorker::UserCtx(iter->first, 0);
   
  auto resData = opIter.prot->writeData(objWrite->getDataBuffer(),
                                 reinterpret_cast<void*>(procCtx));

  if(!resData)
  {
    // failed to write
    delete procCtx;
    opIter.closeContext();
    
    activeOps_.erase(iter);
     
  }
  else
  { // no errors occurred
    procCtx->opID = static_cast<uint32_t>(resData);
    objWrite->updateDataBuffer(resData);
  } 
  
}

void
StoreWorker::processCompletedRadosRead(CephContext::RadosOpCtx* const opCtx){

  // check if a pending read exists
  assert(opCtx); // must not be nullptr
  assert(opCtx->userCtx); // must not be nullptr

  StoreWorker::UserCtx* const procCtx = reinterpret_cast<StoreWorker::UserCtx* const>(opCtx->userCtx);

  assert(procCtx);

  // find an active operation
  auto readIter = activeOps_.find(procCtx->path);
  if(readIter == activeOps_.end())
  {
    //log this as an error
    int a = 5;
    delete procCtx;

    return;
  }

  auto& opIter = readIter->second;

  if(opCtx->opStatus != Status::STAT_SUCCESS)
  { // terminate the operation
    // no need to send anything on failed read (protocol)
    if(opIter.object.validUserContext())
    {
      Task resTask(std::move(opIter.object.getTask(true)));
      resTask.workerID_ = Worker::id_;
      resTask.opStat_   = Status::ERR_INTERNAL;

      // notify the user
      assert(opIter.object.setResponse(std::move(resTask)));

      // close the context since there is no more operations
      opIter.closeContext();
      activeOps_.erase(readIter); // completed op
       
    }//if
    else
    { // means need to wait for the next read op
        if(opIter.pendRequests.empty())
        {
          opIter.object.setObjectOpStatus(Status::ERR_INTERNAL);
        }
        else
        {  // are some pending Read OPs (negate both)
          opIter.closeContext();
          activeOps_.erase(readIter);
        }
    }//else
    
  }// if != STAT_SUCCESS
  else
  { // successfully completed an operation
    // process the request
    ReadObject* readObj = opIter.object.getReadObject(false);
    assert(readObj);
    assert(opIter.insertReadBuffer(std::move(opCtx->opData),
                                     procCtx->opID));

    // set the status accordingly
    //opIter.object.setObjectOpStatus(Status::STAT_PARTIAL_READ);

    if(readObj->validUserContext())
    { 
      processPendingRead(readIter);
    }
    // wait until the next READ operation is issued 
    // by the user
    
  }

  // delete the context
  delete procCtx;
}


void
StoreWorker::processPendingRead(StoreWorker::OpItr& pendItr)
{
  // process pending read operations
  // check if any errors have not occurred
  auto& opIter = pendItr->second;

  if(opIter.object.getObjectOpStatus() != Status::STAT_PARTIAL_READ)
  {
    int a = 5; // log it (something went wrong)
    Task resTask(opIter.object.getTask());
    resTask.workerID_ = Worker::id_;
    resTask.opStat_   = opIter.object.getObjectOpStatus();
    
    opIter.object.setResponse(std::move(resTask));
    opIter.closeContext();
    // remove from active operations
    activeOps_.erase(pendItr);
    // done
  }
  else
  { // Reading is still going on
    // check if it possible to read locally data or need to issue
    // a new Rados operation.
    ReadObject* readObj = opIter.object.getReadObject();
    assert(readObj);

    while(readObj->availableBuffer())
    { // there is some data to be read locally
      Task resTask(readObj->getTask());
      resTask.workerID_ = Worker::id_;
      
      
      const uint64_t bufferSize = static_cast<uint64_t>(resTask.dataSize_);      uint64_t writeBuffer = bufferSize;

      // since it is possible to read from multiple
      // librados::bufferlists, need to loop until the 
      // shared memory has no more space or no more data
      uint64_t readOneTime;
      const char* rawData;
      char* ptr = reinterpret_cast<char*>(resTask.dataAddr_);

      while(readObj->availableBuffer() && writeBuffer > 0)
      {
        readOneTime = writeBuffer;
        rawData = readObj->getRawBytes(readOneTime);
        assert(rawData);
      
        // write data to the shared memory region
        std::memcpy(ptr, rawData, readOneTime);

        // update the values for correct next write
        writeBuffer -= readOneTime;
        ptr += readOneTime;

      }//while

        

      // notify the user about the written data
      resTask.dataSize_ = static_cast<uint32_t>((bufferSize - writeBuffer));


      // done with reading
      if(readObj->isComplete())
      { // final read operation

         resTask.opStat_ = Status::STAT_SUCCESS;
         readObj->setResponse(std::move(resTask));

         // close the operation context
         opIter.closeContext();
         
         //remove the operation
         activeOps_.erase(pendItr);

         return; // done reading 
      }
      else
      {
        resTask.opStat_ = Status::STAT_PARTIAL_READ; // need to read more
        readObj->setResponse(std::move(resTask));

        if(opIter.pendRequests.empty())
        {
          return; // done locally handling requests
        }
        else
        {
          // there are some read issued ops
          readObj->replaceUserContext(std::move(opIter.pendRequests.front()));
          opIter.pendRequests.pop_front();
        }
      }
      
    }// while (locally read data)
   
    // need to issue a new read operation request
    // avoid issueing operations for already in flight 
    // operations
    if(!(opIter.prot->doneReading()))
    {
      // need to read more data
      StoreWorker::UserCtx* userCtx = new StoreWorker::UserCtx(
               pendItr->first, opIter.tranID);

      // update transaction ID
      ++(opIter.tranID);

      auto readBytes = opIter.prot->readData(MAX_IO_SIZE, 
                              reinterpret_cast<void*>(userCtx)); 

      if(readBytes == 0)
      { // internal error
        int a = 5; // log this error
        delete userCtx;
        opIter.closeContext();
        activeOps_.erase(pendItr);
          
      }
    }//if  doneReading()
  }//else (STAT_PARTIAL_READ)
  
}



void
StoreWorker::processReadOp(StoreWorker::UpperRequest&& task)
{
}



void
StoreWorker::issueNewReadOp(StoreWorker::UpperRequest&& task)
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
StoreWorker::processWriteOp(StoreWorker::UpperRequest&& task)
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
                             CommonCode::IOOpCode::OP_WRITE,
                             resTask.objSize_)));


  // appned to the list
  responses_.push_back(std::move(writeTmpCtx));
}


void
StoreWorker::processDeleteOp(StoreWorker::UpperRequest&& task)
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
StoreWorker::executeRadosOps()
{
  auto opIter = responses_.begin();       // for erasing
  decltype(responses_.begin()) prevIter;  // an item
  
  while(opIter != responses_.end())
  {
    // check if the operation is completed
    if(opIter->secRes->isReady())
    { // handle the request and delete this item
   
      // enqueue to the protocol to decipher it
      // and provide a result
      assert(opIter->uppReq); // must not be nullptr
      assert(opIter->secRes); 

      remProt_->handleMessage(
         std::make_shared<IOResponse>(std::move(opIter->secRes->value())),
         opIter->uppReq->second,  *cephCtx_);

      // 
      opIter->uppReq = nullptr; // ensures it's not deleted
      
 
      // prepare deleting the current item
      prevIter = opIter; 
      ++opIter;
      responses_.erase(prevIter);
    }// if

    
  }// while
  
}


void
StoreWorker::sendWriteConfirmation(std::map<std::string, OpContext>::iterator& writeIter)
{
  auto& opIter = writeIter->second;
  const auto& tmpRef = opIter.object.getTask();
  assert(tmpRef != DataObject::empty_task);

  std::string pathVal(tmpRef.path_);
  UserAuth authVal(tmpRef.username_.c_str(), reinterpret_cast<const char*>(workerSecret));
  auto ioRes = std::move(opIter.prot->getCephSuccessResult());
  assert(ioRes); // non null
  
   

}


} // namesapce singaistorageaipc
