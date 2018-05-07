// C++ std
#include <cassert>
#include <cstring>
#include <limits>
#include <algorithm>

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
using singaistorageipc::radosbuffermanagement::DataObject;
using singaistorageipc::radosbuffermanagement::ReadObject;
using singaistorageipc::radosbuffermanagement::WriteObject;

namespace singaistorageipc
{


static bool initializeDataObject(DataObject* obj, 
                                 const CommonCode::IOOpCode opCode,
                                 std::pair<Promise<Task>, Task>&& ctx,
                                 const uint64_t objSize)
{
  switch(opCode)
  {
    case CommonCode::IOOpCode::OP_READ:
    {
      radosbuffermanagement::ReadObject* readObj = new radosbuffermanagement::ReadObject(std::move(ctx));
      readObj->setObjectSize(objSize);
      obj->setReadObject(readObj);
      return true;
    }
  
    case CommonCode::IOOpCode::OP_CHECK_WRITE:
    case CommonCode::IOOpCode::OP_WRITE:
    {
      radosbuffermanagement::WriteObject* writeObj = new radosbuffermanagement::WriteObject(std::move(ctx));
      obj->setWriteObject(writeObj);

      return true;
    }

    default:
      break;

  } //switch

  return false;
}


StoreWorker::OpContext::OpContext()
: prot{nullptr},
  object(),
  tranID(0),
  backID(0),
  totalOpSize(0),
  active(true)
{
  
}


StoreWorker::OpContext::OpContext(std::shared_ptr<RemoteProtocol::ProtocolHandler> ph, DataObject&& ob)
: prot(ph),
  object(std::move(ob)),
  tranID(0),
  backID(0),
  totalOpSize(0),
  active(true)
{}


StoreWorker::OpContext::OpContext(struct StoreWorker::OpContext&& other)
: prot(std::move(other.prot)),
  object(std::move(other.object)),
  tranID(other.tranID),
  backID(other.backID),
  totalOpSize(other.totalOpSize),
  pendData(std::move(other.pendData)),
  pendRequests(std::move(other.pendRequests)),
  issuedOps(std::move(other.issuedOps)),
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
StoreWorker::OpContext::isActive() const noexcept
{
  return active;
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


bool
StoreWorker::OpContext::insertUserCtx(
             struct StoreWorker::UserCtx* const opCtx)
{

  void* opKey = static_cast<void*>(opCtx);
  assert(opKey); // make sure non-null
  auto resPair = issuedOps.emplace(opKey, opCtx);

  assert(resPair.second);

  return resPair.second; // need to make sure is true

}


bool
StoreWorker::OpContext::removeUserCtx(
             struct StoreWorker::UserCtx* const opCtx)
{
  void* opKey = static_cast<void*>(opCtx);
  assert(opKey); // non-nullptr
  
  auto delItr = issuedOps.find(opKey);
  
  if(delItr == issuedOps.end())
  {
    LOG(WARNING) << "StoreWorker::OpContext::removeUserCtx: "
                 << "cannot find user context in the issuedOps.";

    return false;
  }

  // found
  // erase it
  issuedOps.erase(delItr);

  return true; // succeeded
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
    Task resTask(std::move(object.getTask(true)));
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


  // set all Rados Operations
  // to be invalid
  for(auto& acOp : issuedOps)
  {
    acOp.second->valid = false; // mark the operation as
                                // no more valid 
  }

  issuedOps.clear();

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

  // enqueued operations to finish
  for(auto& canTsk : pendTasks_)
  {

    if(canTsk.second.empty())
    {
      continue;
    }

    for(auto& pendTsk : canTsk.second)
    {

      Task tmpTask;
      tmpTask = std::move(pendTsk.second);
      tmpTask.opStat_ = CommonCode::IOStatus::ERR_INTERNAL;

      // notify the user that there was an error
      pendTsk.first.setValue(std::move(tmpTask));
    }//for

    canTsk.second.clear();
   
  }//for


  // clear memory
  pendTasks_.clear();

  
  // all active operations
  for(auto& canTsk : activeOps_)
  {
    // close all the contexts
    canTsk.second.closeContext();
  }

  activeOps_.clear(); // clear all active operations



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
  initSha = SHA256_Update(&shaCtx, 
                          reinterpret_cast<const unsigned char*>\
                          (WORKER_SHARED_SECRET), 
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

  // set the last character to '\0'
  workerSecret[SHA256_DIGEST_LENGTH] = '\0'; // terminate string

  
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
       // use the worker ID of this worker
       tmpTask.second.workerID_ = Worker::id_; 
 
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

        case CommonCode::IOOpCode::OP_CHECK_WRITE:
        {
          processNewWriteOp(std::move(tmpTask));
          
          break; // one task has been completed
        }

        case CommonCode::IOOpCode::OP_ABORT:
		{
          processAbortOp(std::move(tmpTask));
			
          break; // one task has been completed
		}

        case CommonCode::IOOpCode::OP_CLOSE:
        {
          processCloseOp(std::move(tmpTask));

          break; // one task has been processed
        }

        case CommonCode::IOOpCode::OP_EXIT:
        {
          // signal to terminate the worker
          Worker::done_.store(true, std::memory_order_release);
          
          goto store_close_label; // leave the loop as
                                  // soon as possible

          break; // one op completed 
        }

        default:
        {
          DLOG(WARNING) << "StoreWorker::processTasks: "
          << "switch found no operation.";
  
          break;
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


// jump label for fast termination
store_close_label:

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
StoreWorker::handlePendingTasks()
{

  // iterate over all pending tasks
  // and try to issue as many as possible
  decltype(activeOps_.begin()) opIter;
  StoreWorker::OpCode opCode;
  bool ctxRes;


  for(auto& pendOp : pendTasks_)
  {
    opIter = activeOps_.find(pendOp.first);

    if(opIter != activeOps_.end() || pendOp.second.empty()) 
    {
      // continue processing other pending tasks
      continue;
    }//if

    auto& headOp = pendOp.second.front(); // get a reference to
                                          // the current operation

    opCode = headOp.second.opType_;

    ctxRes = createOperationContext(headOp.second.path_,
                                    opCode, headOp.second.sock_);

    if(!ctxRes)
    {
      continue; // do next time
    }

    sendRequestToServer(std::move(headOp), opCode);

    // discard the operation
    pendOp.second.pop_front();

  }//for


  // discard all empty lists
  auto delBeg = pendTasks_.cbegin();
  decltype(pendTasks_.cbegin()) prevItr;

  while(delBeg != pendTasks_.cend())
  {
    if(delBeg->second.empty())
    {
     prevItr = delBeg;
     ++delBeg;
     pendTasks_.erase(prevItr);
    }
    else
    {
      ++delBeg;
    }
  }//while
}


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
          LOG(ERROR) << "StoreWorker::handleActiveOps: "
                     << "opCtx->opType neihter OP_WRITE nor "
                     << "OP_READ";
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

  StoreWorker::UserCtx* const procCtx = static_cast<StoreWorker::UserCtx* const>(opCtx->userCtx);

  assert(procCtx);


  // need to make sure that this Rados operation
  // is still valid
  if(!procCtx->valid)
  { // the IO operation has been cancelled
    delete procCtx; // handled the operation
    return; 
  }


  //find an active write operation
  auto writeIter = activeOps_.find(procCtx->path);
  const uint64_t writeOpSize = static_cast<const uint64_t>(procCtx->opID);
 

  if(writeIter == activeOps_.end())
  {
    LOG(ERROR) << "StoreWorker::processCompletedRadosWrite: " 
               << "writeIter == activeOps_.end()";

    delete procCtx; 
    return;
  }

  // found an active write operation
  auto& opIter = writeIter->second;


  // remove the user context from the list
  // of pending Rados operations
  opIter.removeUserCtx(procCtx);
  delete procCtx;


  if(opCtx->opStatus != Status::STAT_SUCCESS)
  { // terminate the operation
    // no need to send anything on failed write (protocol)

    // WRITE operation must have at least one active
    // user request
    assert(opIter.object.validUserContext()); 

    Task resTask(std::move(opIter.object.getTask(true)));
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
    opIter.totalOpSize -= writeOpSize;
    if(opIter.prot->doneWriting() && opIter.totalOpSize == 0)
    {
      // completed writing 
      // send a response to the auth server
      sendWriteConfirmation(writeIter);
      // change object state
      // waiting for closing this operation
      opIter.object.setObjectOpStatus(Status::STAT_CLOSE);
    }
    else
    {
      if(opIter.object.isComplete())
      {
        // get a write operation and send PARTIAL WRITE complete
        Task resTask(std::move(opIter.object.getTask(true)));
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

     // issue merge signal (pass the ID of the merging OP)
     Task resTask(std::move(possibleTask));
     resTask.opStat_  = Status::STAT_PARTIAL_WRITE;
     resTask.mergeID_ = task.tranID_; // merge multiple
                                      // WRITES
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
  StoreWorker::UserCtx* const procCtx = new StoreWorker::UserCtx(iter->first, 0, true);
   
  auto resData = opIter.prot->writeData(objWrite->getDataBuffer(),
                                 static_cast<void*>(procCtx));

  if(!resData)
  {
    // failed to write
    delete procCtx;
    opIter.closeContext();
    
    activeOps_.erase(iter);
     
  }
  else
  { // no errors occurred
    // safely can cast since Raods cannot write more than 
    // 4GB of data at once
    procCtx->opID = static_cast<uint32_t>(resData); 
    objWrite->updateDataBuffer(resData);

    // enqueue the context to the list
    // of active Rados ops
    opIter.insertUserCtx(procCtx);
  } 
  
}

void
StoreWorker::processCompletedRadosRead(CephContext::RadosOpCtx* const opCtx){

  // check if a pending read exists
  assert(opCtx); // must not be nullptr
  assert(opCtx->userCtx); // must not be nullptr

  StoreWorker::UserCtx* const procCtx = static_cast<StoreWorker::UserCtx* const>(opCtx->userCtx);

  assert(procCtx);

  // might be that the Rados Operation
  // has been cancelled
  if(!procCtx->valid)
  {
    // no need to handle this Rados Operation
    delete procCtx;
    return;
  }


  // find an active operation
  auto readIter = activeOps_.find(procCtx->path);
  if(readIter == activeOps_.end())
  {
    //log this as an error(may not be an error if another Op deleted)
    LOG(ERROR) << "StoreWorker::processCompletedRadosRead: "
               << "readIter == activeOps_.end()";
    delete procCtx;

    return;
  }

  auto& opIter = readIter->second;
  // remove the context from the map of issued Rados ops
  opIter.removeUserCtx(procCtx);


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
    DLOG(WARNING) << "StoreWorker::processPendingRead: "
                  << "opIter.object.getObjectOpStatus() != "
                  << "Status::STAT_PARTIAL_READ";

    Task resTask(opIter.object.getTask());
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
    ReadObject* readObj = opIter.object.getReadObject(false);
    assert(readObj);

    while(readObj->availableBuffer())
    { // there is some data to be read locally
      Task resTask(std::move(readObj->getTask(true)));
      
      
      const uint64_t bufferSize = static_cast<const uint64_t>(resTask.dataSize_);      
      uint64_t writeBuffer = bufferSize;

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

         resTask.opStat_   = Status::STAT_SUCCESS;
         resTask.objSize_  = 0; // no more reading (data)
         readObj->setResponse(std::move(resTask));

         // close the operation context
         opIter.closeContext();
         
         //remove the operation
         activeOps_.erase(pendItr);

         return; // done reading 
      }
      else
      {
        resTask.opStat_  = Status::STAT_PARTIAL_READ; // need to read more
        resTask.objSize_ = readObj->getRemainingObjectSize();
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
               pendItr->first, opIter.tranID, true);

      // update transaction ID
      ++(opIter.tranID);

      auto readBytes = opIter.prot->readData(MAX_IO_SIZE, 
                              static_cast<void*>(userCtx)); 

      if(readBytes == 0)
      { // internal error
        LOG(ERROR) << "StoreWorker::processPendingRead: "
                   << "opIter.prot->readData() == 0";
        delete userCtx;
        opIter.closeContext();
        activeOps_.erase(pendItr);
          
      }
      else
      {
        // need to enqueue the user context
        opIter.insertUserCtx(userCtx);
      }
    }//if  doneReading()
  }//else (STAT_PARTIAL_READ)
  
}



void
StoreWorker::notifyUserStatus(StoreWorker::UpperRequest&& req, 
                             const StoreWorker::Status stat)
{
  // just simple error notification
  Task probTask(std::move(req.second));
  probTask.opStat_ = stat;
  req.first.setValue(std::move(probTask));
}


bool
StoreWorker::checkOpAuth(const Task& opTask, const Task& valTask)
{

  return (opTask.username_ == valTask.username_ && 
          opTask.sock_ == opTask.sock_);

}


bool
StoreWorker::createOperationContext(const std::string& pathVal,
                                    const StoreWorker::OpCode opType,
                                    const int sockFd)
{
  auto insRes = activeOps_.emplace(std::make_pair(
                                   std::string(pathVal),
                                   StoreWorker::OpContext()));

  if(insRes.second)
  {
    // keep a reference to the socket
    insRes.first->second.totalOpSize = static_cast<const uint64_t>(sockFd);
  }

  return insRes.second;
}

void
StoreWorker::sendRequestToServer(StoreWorker::UpperRequest&& task,
                                 const StoreWorker::OpCode opType)
{

  // send request to the server
  std::string tmpDataPath(task.second.path_);
  UserAuth tmpAuth(task.second.username_.c_str(),
                   reinterpret_cast<const char*>(workerSecret));
  

  StoreWorker::WorkerContext tmpWorkCtx(
                               std::move(task),
                               std::move(
                                 secure_->checkPerm(
                                   std::move(tmpDataPath), 
                                   std::move(tmpAuth),
                                   opType,
                                   task.second.objSize_)));


  // append to the list
  responses_.push_back(std::move(tmpWorkCtx));
}



void
StoreWorker::processWriteOp(StoreWorker::UpperRequest&& task)
{
  // need to find an active write operation; otherwise,
  // reject the operation.
  auto ctxIter = activeOps_.find(task.second.path_);
  if(ctxIter == activeOps_.end())
  {
    //error (reject operation)
    notifyUserStatus(std::move(task), StoreWorker::Status::ERR_DENY);
  }
  else
  {
    // found an active write operation
    auto& opIter = ctxIter->second;

    // need to make sure that the same user
    // is accessing
    if(opIter.object.getObjectOpType() != OpCode::OP_WRITE
       || !checkOpAuth(task.second, opIter.object.getTask()))
    {
      notifyUserStatus(std::move(task), Status::ERR_DENY);
      return;
    }    

    // append the context to the operation
    if(opIter.object.validUserContext())
    {
      opIter.appendRequest(std::move(task));
    }
    else
    {
      opIter.object.replaceUserContext(std::move(task));
    }
  }
}



void
StoreWorker::processReadOp(StoreWorker::UpperRequest&& task)
{
  // need to find an active write operation; otherwise,
  // reject the operation.
  auto ctxIter = activeOps_.find(task.second.path_);
  if(ctxIter == activeOps_.end())
  {
    // need to issue a new READ request
    auto opIter = pendTasks_.find(task.second.path_);


    if(opIter == pendTasks_.end())
    { // insert a new list of pending ops for this data object
      std::string pathVal(task.second.path_);
      std::list<UpperRequest> tmpList;
      auto insRes = pendTasks_.emplace(std::move(pathVal), std::move(tmpList));
      assert(insRes.second);
      opIter = insRes.first; // set iterator to point at the new list

    }

    opIter->second.push_back(std::move(task));


  }
  else
  {
    // found an active read operation
    auto& opIter = ctxIter->second;

    // need to make sure that the same user
    // is accessing
    if(opIter.object.getObjectOpType() != OpCode::OP_READ
       || !checkOpAuth(task.second, opIter.object.getTask()))
    {
      notifyUserStatus(std::move(task), Status::ERR_DENY);
      return;
    }   

    // set the read as a pending read
    opIter.object.setObjectOpStatus(Status::STAT_PARTIAL_READ);

    // append the context to the operation
    if(opIter.object.validUserContext())
    {
      opIter.appendRequest(std::move(task));
    }
    else
    {
      opIter.object.replaceUserContext(std::move(task));
    }
  }
}



void
StoreWorker::processDeleteOp(StoreWorker::UpperRequest&& task)
{

  // just append the operation to the pending deque
  auto opIter = pendTasks_.find(task.second.path_);
  if(opIter == pendTasks_.end())
  { // insert a deque
    std::string pathVal(task.second.path_);
    std::list<UpperRequest> tmpList;
    auto insRes = pendTasks_.emplace(std:: move(pathVal), std::move(tmpList));
    assert(insRes.second);
    opIter= insRes.first;
  }

  opIter->second.push_back(std::move(task));

}


void
StoreWorker::processAbortOp(StoreWorker::UpperRequest&& task)
{
  // need to find any pending operation
  auto iterVal = activeOps_.find(task.second.path_);
  
  if(iterVal == activeOps_.end())
  {
    DLOG(WARNING) << "StoreWorker::processAbortOp: "
                  << "data path has not been found";

    notifyUserStatus(std::move(task), Status::ERR_INTERNAL);
  }
  else
  {
    // need to terminate all the operations
    auto& opIter = iterVal->second;
    
    // check if the termination matches
    // the operation tuple
    if(!checkOpAuth(task.second, opIter.object.getTask()))
    { // not authorized to abort
      notifyUserStatus(std::move(task), Status::ERR_DENY);
    }
    else
    { // terminate the operation
      opIter.closeContext();
      activeOps_.erase(iterVal);
      notifyUserStatus(std::move(task), Status::STAT_SUCCESS);
    }
    
  }

}


void
StoreWorker::processCloseOp(StoreWorker::UpperRequest&& task)
{

  // first need to make sure to terminate
  // an active op
  auto pendOp = activeOps_.find(task.second.path_);
  if(pendOp != std::end(activeOps_))
  {
    // need to make sure that the operation
    // can be closed  
    bool closeOp = false;
    try
    {
      closeOp = checkOpAuth(task.second, pendOp->second.object.getTask());
    }
    catch(const std::exception& exp)
    {
      // no task has been found
      // check only the socket
      const int sockfd = static_cast<const int>(pendOp->second.totalOpSize);
      closeOp = (sockfd == task.second.sock_);
    }
    
    if(closeOp)
    { // need to close the operation
      pendOp->second.closeContext();
    }

  }

  // socker id
  const int sockKey = task.second.sock_;

  // iterate over the map and
  // delete all request with sock_
  auto curMap = pendTasks_.begin();
  decltype(pendTasks_.begin()) prevMap;

  while(curMap != pendTasks_.end())
  {
    auto begList  = curMap->second.begin();
    auto prevList = curMap->second.begin();
    // iterate over a list and remove all
    // requests with sock_

    while(begList != curMap->second.end())
    {
      if(sockKey == begList->second.sock_)
      { // delete this request
        prevList = begList;
        ++begList;
        notifyUserStatus(std::move(*prevList), Status::STAT_CLOSE);

        curMap->second.erase(prevList);
      }
      else
      {
        // just increment the iterator
        ++begList;
      }
    } //while

    // check if the list becomes empty
    if(curMap->second.empty())
    { // delete it
      prevMap = curMap;
      ++curMap;
      pendTasks_.erase(prevMap);
    }
    else
    { // move to the next list
      ++curMap;
    }

  }//while

  // done processing all the 
  // requests, just notify the original close 
  // task as stat close
  notifyUserStatus(std::move(task), Status::STAT_CLOSE);
    
}


void
StoreWorker::processNewWriteOp(StoreWorker::UpperRequest&& task)
{

  // just append the operation to the pending deque
  auto opIter = pendTasks_.find(task.second.path_);
  if(opIter == pendTasks_.end())
  { // insert a deque
    std::string pathVal(task.second.path_);
    std::list<UpperRequest> tmpList;
    auto insRes = pendTasks_.emplace(std::move(pathVal), std::move(tmpList));
    assert(insRes.second);
    opIter= insRes.first;
  }

  opIter->second.push_back(std::move(task));

}


void
StoreWorker::handleRemoteResponses()
{
  auto opIter = responses_.begin(); // iterate over the list
                                    // and delete some items
  decltype(responses_.begin()) prevIter;
  
  while(opIter != responses_.end())
  {
    // check if the operation is completed
    if(opIter->secRes->isReady())
    { // handle the request and delete this item
   
      // enqueue to the protocol to decipher it
      // and provide a result
      assert(opIter->uppReq); // must not be nullptr
      assert(opIter->secRes); 

      
 
      auto res = remProt_->handleMessage(
         std::make_shared<IOResponse>(std::move(opIter->secRes->value())),
         opIter->uppReq->second,  *cephCtx_);

      //can handle fast?
      if(res->getStatusCode() != Status::STAT_SUCCESS)
      {
        // send an error to the user
        Task remErrTask(std::move(opIter->uppReq->second));
        remErrTask.opStat_ = res->getStatusCode();

        // remove a pending operation is it exists
        auto pendIter = activeOps_.find(remErrTask.path_);
        if(pendIter != activeOps_.end())
        {
          // remove it
          activeOps_.erase(pendIter);
        }

        opIter->uppReq->first.setValue(std::move(remErrTask));
        // done with this operation
      }
      else
      {
        processRemoteResult(opIter, res);
      }

      // delete the item from the list
      prevIter = opIter; 
      ++opIter; // update the iterator for deletion
      responses_.erase(prevIter); 

    }// if
    else
    {
      ++opIter; // update the iterator  
    }  
  }// while
 
}


void
StoreWorker::processRemoteResult(std::list<StoreWorker::WorkerContext>::iterator& resIter, std::shared_ptr<RemoteProtocol::ProtocolHandler> handler)
{

 
 if(handler->needCephIO())
 {
   // operation context must have been
   // preallocated
   auto pendOpCtx = activeOps_.find(resIter->uppReq->second.path_);
   assert(pendOpCtx != activeOps_.end());

   auto& ctxItr = pendOpCtx->second; // get reference

   // need to check the operation context is still
   // active
   if(!ctxItr.isActive())
   { // has been terminated locally
     notifyUserStatus(std::move(*(resIter->uppReq)), Status::STAT_CLOSE);
     activeOps_.erase(pendOpCtx); // no more operations
     return;
   }



   ctxItr.prot = handler; // reset the handler
   ctxItr.totalOpSize = handler->getTotalDataSize(); 

   auto initRes = initializeDataObject(&(ctxItr.object),
                                       handler->getOpType(),
                                       std::move(*(resIter->uppReq)),
                                       handler->getTotalDataSize());

   assert(initRes);
   // need to inform the user about write success
   if(handler->getOpType() == OpCode::OP_CHECK_WRITE)
   {
     Task permTask(std::move(ctxItr.object.getTask(true)));
     permTask.opStat_ = handler->getStatusCode();
     ctxItr.object.setResponse(std::move(permTask));
   } // done

  
 }
 else
 {
   // check for delete or connnect operations
   switch(handler->getOpType())
   {
     case OpCode::OP_DELETE:
     {
       // notify the user about the success
       Task delSuccess(std::move(resIter->uppReq->second));
       delSuccess.opStat_ = Status::STAT_SUCCESS;
       auto pendOpCtx = activeOps_.find(delSuccess.path_);
       assert(pendOpCtx != activeOps_.end());

       // need to check if the context
       // is still active
       if(!pendOpCtx->second.isActive())
       {
         // notify closed successfully
         delSuccess.opStat_ = Status::STAT_CLOSE;
       }

       // remove the operation from active ones
       activeOps_.erase(pendOpCtx);

       resIter->uppReq->first.setValue(std::move(delSuccess));

       break;
     }
     case OpCode::OP_AUTH:
     { 
       // handle authentication
       Task authSuccess(std::move(resIter->uppReq->second));
       authSuccess.opStat_ = Status::STAT_SUCCESS;
       authSuccess.username_ = handler->getValue("Username");
       assert(authSuccess.username_ != RemoteProtocol::ProtocolHandler::empty_value);

       resIter->uppReq->first.setValue(std::move(authSuccess));

       break;
     }
    
     case OpCode::OP_COMMIT:
     {
       //means finished a write operation and need to commit
       // the result
       Task commitSuccess(std::move(resIter->uppReq->second));
       commitSuccess.opStat_ = Status::STAT_SUCCESS;

       auto pendOpCtx = activeOps_.find(commitSuccess.path_);
       assert(pendOpCtx != activeOps_.end());
       assert(pendOpCtx->second.object.getObjectOpStatus() == Status::STAT_CLOSE);

       // if the operation has been closed
       if(!pendOpCtx->second.isActive())
       {
         // closed
         commitSuccess.opStat_ = Status::STAT_CLOSE;
       }

       // delete the operation
       activeOps_.erase(pendOpCtx);
       resIter->uppReq->first.setValue(std::move(commitSuccess)); // confirm success
       
       break;
     }  
    
     default:
     {
       LOG(ERROR) << "StoreWorker::processRemoteResult: "
                  << "no valid opType";
       // reply with a failure 
       Task errReply(std::move(resIter->uppReq->second));      
       errReply.opStat_ = Status::ERR_DENY;
       // remove if any active ops
       auto opAcItr = activeOps_.find(errReply.path_);
       if(opAcItr != activeOps_.end())
       {
         //remove it
         activeOps_.erase(opAcItr);
       }
       resIter->uppReq->first.setValue(std::move(errReply));

       break;
     }
   }//switch
 }//else

}


void
StoreWorker::sendWriteConfirmation(std::map<std::string, OpContext>::iterator& writeIter)
{
  auto& opIter = writeIter->second;
  
  assert(opIter.object.validUserContext());

  // initialize the context
  auto& userCtx = opIter.object.getUserContext(true);
  std::string pathVal(userCtx.second.path_);

  // initialize the required remote values for auth server
  UserAuth authVal(userCtx.second.username_.c_str(), reinterpret_cast<const char*>(workerSecret));
  auto writeResult  = std::move(opIter.prot->getCephSuccessResult());
  assert(writeResult); // non null
  auto opTmpCode = writeResult->msg_->opType_; 
 
  StoreWorker::WorkerContext confirmTmpCtx(std::move(userCtx),
                                  std::move(
                                  secure_->sendIOResult(
                                    std::move(pathVal),
                                    std::move(authVal),
                                    opTmpCode,
                                    std::move(*(writeResult.release()))
                                  )));


  responses_.push_back(std::move(confirmTmpCtx));
}


} // namesapce singaistorageaipc
