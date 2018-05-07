

// C++ std lib
#include <limits>
#include <exception>



// Google libs
#include <glog/logging.h>

// Project lib
#include "include/JSONHandler.h"
#include "SecurityModule.h"


using folly::Future;
using folly::Promise;


namespace singaistorageipc
{

static constexpr const uint32_t MAX_U32_VALUE = std::numeric_limits<uint32_t>::max(); // size of uint32_t
static constexpr const uint32_t U32_ONE = static_cast<uint32_t>(1);
static constexpr const uint32_t U32_ZERO = static_cast<uint32_t>(0);


// helper function for determining if the security module can  issue
// another request to the channel.
static uint32_t getWindowSize(const uint32_t headSeq, 
                              const uint32_t backSeq)
{
 
  // check which case has to be considered
  if(headSeq > backSeq)
  {
    return (MAX_U32_VALUE -  headSeq + backSeq + U32_ONE);
  }
  else
  {
    return (backSeq - headSeq);
  }

}



SecurityModule::FollySocketCallback::FollySocketCallback(SecurityModule* sec, const uint32_t tranID)
: secMod_(sec),
  tranID_(tranID)
{}


void
SecurityModule::FollySocketCallback::writeSuccess() noexcept
{
  DLOG(INFO) << "Sent a message to a remote server";
}

void
SecurityModule::FollySocketCallback::writeErr(
                         const size_t bytesWritten,
                         const folly::AsyncSocketException& exp) noexcept
{

  LOG(WARNING) << "SecurityModule::FollySocketCallback::writeErr: "
               << "bytesWritten: " << bytesWritten
               << ", err_type: " << exp.getType() 
               << ", err_code: " << exp.getErrno(); 

  // enqueue the failure to the failure queue
  secMod_->socketError(tranID_);
}


SecurityModule::SecurityModule(std::unique_ptr<ServerChannel>&& comm)
: Security(std::move(comm)),
  nextID_(0),
  backID_(0),
  active_(false)
{}



SecurityModule::~SecurityModule()
{

}


Future<Task>
SecurityModule::clientConnect(const UserAuth& user)
{

  std::shared_ptr<Request> authMsg = std::make_shared<Request>(
                                     0, user.username_, 
                                     user.passwd_,
                                     std::string(""), 
                                 CommonCode::IOOpCode::OP_AUTH);

  Promise<Task>* prom = new Promise<Task>();
  auto futRes = prom->getFuture(); // for async computation

  TaskWrapper tmpWrap;
  tmpWrap.msg_     = std::move(authMsg);
  tmpWrap.resType_ = SecurityModule::MessageSource::MSG_SERVER;
  tmpWrap.futRes.serverTask_ = prom;

  
  // try to enqueue the task
  tasks_.push(std::move(tmpWrap));


  // return the future 
  return (std::move(futRes));
}


Future<IOResponse>
SecurityModule::checkPerm(const std::string& dataPath,
                          const UserAuth&    user,
                          const CommonCode::IOOpCode op,
                          const uint64_t dataSize)
{
 
  std::shared_ptr<Request> dataMsg = std::make_shared<Request>(
                                      0, 
                                      user.username_, user.passwd_,
                                      dataPath, op);

  Promise<IOResponse>* prom = new Promise<IOResponse>();
  auto futRes = prom->getFuture(); // for async computation

  TaskWrapper tmpWrap;
  tmpWrap.msg_     = std::move(dataMsg);
  tmpWrap.resType_ = SecurityModule::MessageSource::MSG_WORKER;
  tmpWrap.futRes.workerTask_ = prom;

  
  // try to enqueue the task
  tasks_.push(std::move(tmpWrap));
  

  // return the future
  return (std::move(futRes));
}


Future<IOResponse>
SecurityModule::sendIOResult(const std::string& dataPath,
                             const UserAuth&    user,
                             const CommonCode::IOOpCode op,
                             const IOResult&  res)
{
  // do not support currently since it involves a 
  // lot of copying (need to copy data)
  throw std::exception(); // throw an exception
}


Future<IOResponse>
SecurityModule::sendIOResult(std::string&& dataPath,
                             UserAuth&&    user,
                             const CommonCode::IOOpCode op,
                             IOResult&&  res)
{

  // convert a unique pointer into a shared one
  std::shared_ptr<Request> dataMsg = std::move(res.msg_);
    
  // reinitialize some values
  dataMsg->userID_     = std::move(user.username_);
  dataMsg->password_   = std::move(user.passwd_);
  dataMsg->opType_     = op;
  dataMsg->objectPath_ = std::move(dataPath);


  Promise<IOResponse>* prom = new Promise<IOResponse>();

  auto futRes = prom->getFuture();
  
  TaskWrapper tmpWrap;
  tmpWrap.msg_ = std::move(dataMsg);
  tmpWrap.resType_ = SecurityModule::MessageSource::MSG_WORKER;
  tmpWrap.futRes.workerTask_ = prom;

  // enqueue the task
  tasks_.push(std::move(tmpWrap));

  // return future
  return futRes;
  
}



void
SecurityModule::processRequests()
{


  while(!Security::done_.load(std::memory_order_relaxed))
  {
    //process the requests
    // try to get as many tasks as possible
    tasks_.dequeue(pendTrans_);
    
    if(pendTrans_.empty()) // nothing to process
    {
      // check if the system has any sent requests
      if(responses_.empty()) // no sent request
      {

        // try to handle any socket errors
        tryHandleSocketErrors();


        // wait for a newly incoming task
        pendTrans_.push_back(std::move(tasks_.pop()));
        // process the new task
        processClientTasks();
      
      }
      else
      {
        // query if any already sent requests have completed
        queryServerChannel();
      }
    
    }
    else // process the client tasks
    {
      processClientTasks();
    }

  }// while


  // release the system resources
  closeSecurityModule();

}


void
SecurityModule::closeSecurityModule()
{


  // close the channel
  // (the channel must close all its sockets before)
  channel_->closeChannel();


  // go over all pending reqeusts firs and reply them
  for(auto& pendTask : pendTrans_)
  {
    handleInternalError(pendTask, CommonCode::IOStatus::ERR_INTERNAL);
  }

  pendTrans_.clear(); // clear the pending transactions


  // now handle all error tasks
  tryHandleSocketErrors();


  // query the channel (last messages are returned)
  queryServerChannel();

  complTrans_.clear();
  responses_.clear();


  // mark the service as completed
  {
    std::lock_guard<std::mutex> tmpLock(termLock_);
    active_ = false;
    termVar_.notify_one(); // notify a thread
  } // lock scope
}




void
SecurityModule::processClientTasks()
{
  // dequeue all task from recvTasks_ and
  // categorize them into either new tasks or
  // pending ones
  uint32_t windowSize = MAX_U32_VALUE;

  if(nextID_ != backID_)
  { // there are some active tasks
    windowSize = getWindowSize(nextID_, backID_);  
  }


  bool fullWindowSize = (windowSize == U32_ZERO);

  while(fullWindowSize) // should never happen
  {
    LOG(WARNING) << "SecurityModule::processNewTasks: window is full";

    // the window may be full due to socket errors 
    const bool foundErrors = tryHandleSocketErrors(); 
    
    if(!foundErrors)
    {
      return; // don't progress further (NO more transactions)

    }
    else
    { 
       // try one more time to check the window size
       windowSize = getWindowSize(backID_, nextID_);
       fullWindowSize = (windowSize == U32_ZERO); 
    }
  
  } // while


  // got some transactions to issue  
  bool sendRes = true;


  while(windowSize > U32_ZERO && !pendTrans_.empty())
  { // keep issueing tasks

    auto issTask = std::move(pendTrans_.front()); // get reference 
                                                  // to the first
                                                  // item

    // remove the task
    pendTrans_.pop_front(); 

    // need to make sure
    // that it's not a terminal message
    if(issTask.resType_ == SecurityModule::MessageSource::MSG_TERM)
    {
      return; // terminate as soon as possible
    }


    // enqueue the message
    issTask.msg_->tranID_ = nextID_;
      
    sendRes = channel_->sendMessage(issTask.msg_, 
                          new FollySocketCallback(this, 
                                                  nextID_));
  
    if(sendRes)
    {
     // enqueue the request
     auto resPair = responses_.emplace(nextID_, std::move(issTask));
     assert(resPair.second); // make sure smoothly appended
   
     ++nextID_;    // update transaction ID
     --windowSize; // update the window size  
    
    }
    else // handle the error
    {
      handleInternalError(issTask, CommonCode::IOStatus::ERR_INTERNAL);

      // no need to explicitly delete the task since
      // the destructor handles it
       
    }// else
    
  }// while



  // query the server
  queryServerChannel();

}



void
SecurityModule::handleInternalError(TaskWrapper& errTask, 
                                    const CommonCode::IOStatus errNo) const
{
  // inform the caller about an internal erorr.
  
  switch(errTask.resType_)
  {
    case MessageSource::MSG_SERVER:
    { // reply to the server
      Task errTaskServer("", errNo); // set the error
      errTaskServer.opType_ = errTask.msg_->opType_;
      
      // notify the server about an internal error
      errTask.futRes.serverTask_->setValue(std::move(errTaskServer));

      break;
    } // case MSG_SERVER
    case MessageSource::MSG_WORKER:
    {
      IOResponse errTaskWorker;
      errTaskWorker.opType_ = errTask.msg_->opType_;
      errTaskWorker.opStat_ = errNo; // set the error
      
      // notify a worker about an internal error
      errTask.futRes.workerTask_->setValue(std::move(errTaskWorker));

      break; // done
    } // case MSG_WORKER
   
    default:
    {
      // shall never happen (need to log though)
      break;
    }

  } // switch

}


void
SecurityModule::queryServerChannel()
{
  // query if the server has received any new messages
  auto newReses = std::move(channel_->pollReadMessage());

  if(!newReses.empty())
  { 
    // process the received messages

    decltype(responses_.begin()) mapIter;
    uint32_t reqID = U32_ZERO;

    for(auto& recVal : newReses)
    {
      mapIter = responses_.find(recVal->tranID_);
       
      if(mapIter == responses_.end()) // log the event
      {
        LOG(ERROR) << "Received message fron the auth server has invalid tranID";
      }
      else
      {
        // there must be one unique key 
        // check  which kind of reponse need to issue
        reqID = recVal->tranID_;

        switch(mapIter->second.resType_)
        {
          case MessageSource::MSG_SERVER:
          {
            processServerResponse(mapIter->second, std::move(recVal));
            break;
          }
          case MessageSource::MSG_WORKER:
          {
            processWorkerResponse(mapIter->second, std::move(recVal));
            break;
          }
          default:
          {
            // log the event
            LOG(ERROR) << "Forgot to set a request type (neither MSG_SERVER nor MSG_WORKER)";
          }
        } // switch


        // now handle the internal structures
        completedTransaction(reqID);
        responses_.erase(mapIter);
      } // else      

    } // for 
  } // if

  
  // finished processing messages
  
}


void
SecurityModule::completedTransaction(const uint32_t tranID)
{
  if(backID_ == tranID) // oldest transaction completed
  {
    // update backID until backID_ becomes nextID_
    // or a gap is found
    ++backID_;
     
    decltype(complTrans_.begin()) mapIter;

    while(backID_ != nextID_ && !complTrans_.empty())
    {
      mapIter = complTrans_.find(backID_);

      if(mapIter == complTrans_.end())
      {
        return; // there is a gap (wait to fill it in)
      }

      ++backID_; // update the backID_
      complTrans_.erase(mapIter); // remove the item from the map

    } // while    
    
  }//if
  else
  {
    // just mark the transaction as a completed one
    auto resPair = complTrans_.emplace(std::make_pair(tranID, true));
    if(!resPair.second) // didn't suceed to insert
    {
      LOG(ERROR) << "Cannot emplace a completed transaction into map";
    }
  }// else
  
}



void
SecurityModule::processWorkerResponse(TaskWrapper& taskRef,
                           std::unique_ptr<Response>&& resValue)
{

  // pass the content to a worker
  IOResponse workerRes;
  workerRes.opType_ = taskRef.msg_->opType_;
  workerRes.opStat_ = resValue->stateCode_;
  Response* tmp = resValue.release();
  std::unique_ptr<Message> tmpPtr = std::make_unique<Response>(std::move(*tmp));
  workerRes.msg_.swap(tmpPtr);
  delete tmp; // nothing left
   

  // notify a worker about the received reponse
  taskRef.futRes.workerTask_->setValue(std::move(workerRes));
}


void
SecurityModule::processServerResponse(TaskWrapper& taskRef, 
                             std::unique_ptr<Response>&& resValue)
{

   if(resValue->msgEnc_ != Message::MessageEncoding::JSON_ENC)
   {
     LOG(WARNING) << "Message uses non-JSON encoding. Message is being discarded";
     return;
   }


  // pass the content to a worker
  Task serverRes(taskRef.msg_->userID_, resValue->stateCode_);
  if(resValue->stateCode_ != CommonCode::IOStatus::ERR_INTERNAL)
  {
    // need to decode the received content of the message
    JSONDecoder<const uint8_t*> jsonDec;

    // try to decode the content
    const bool decRes = jsonDec.decode(resValue->data_->data());
    if(!decRes)
    { // set to internal error
      // need to log this
      LOG(WARNING) << "Decoder cannot decode a JSON Message from the server";
      serverRes.opStat_ = CommonCode::IOStatus::ERR_INTERNAL;

    }//if
    else
    {
      bool internalErr = false;
      // try to retrieve the results
      auto& jsonObj = jsonDec.getResult();
 

      try
      {
        auto resIter =  jsonObj.getObject("Result");

        // now need to try to find either 'Account'
        // or 'Error_Type'
        try
        {
          auto accName = resIter->getValue<std::string>("Account");
          
          // found account
          serverRes.username_ = std::move(accName);
          serverRes.opStat_   = CommonCode::IOStatus::STAT_SUCCESS;
          
        }catch(const std::exception& exp)
        {
          internalErr = true;
        }

      if(internalErr) // means could not find Account
                      // try to find error code
      {
        try
        {
          auto errType = resIter->getValue<uint64_t>("Error_Type");

          // try to convert server error type into
          // local error type          

          serverRes.opStat_   = static_cast<CommonCode::IOStatus>(errType);
          internalErr = false;  // no errors
        }
        catch(const std::exception& exp)
        {
          //final error
          internalErr = true;
        }
        
     
      }// if

       

      }catch(const std::exception& exp)
      {
        internalErr = true; // some error occured
      }

     if(internalErr)
     { // some internal error occured
       LOG(WARNING) << "Failed to decode JSON Message from auth server";
       serverRes.opStat_ = CommonCode::IOStatus::ERR_INTERNAL;
     }

    }//else  

  }//if   

  // notify a worker about the received reponse
  taskRef.futRes.serverTask_->setValue(std::move(serverRes));

}


void
SecurityModule::socketError(const uint32_t errTranID)
{
  std::lock_guard<std::mutex> tmpLock(errLock_);
  // just append the transaction to the failed operations
  sockErrs_.push_back(errTranID);

}


bool
SecurityModule::tryHandleSocketErrors()
{
  // try to dequeue any socket errors
  std::vector<uint32_t> sockErrors;

  dequeueSocketErrors(sockErrors);

  if(sockErrors.empty())
  {
    return false; // no socket errors have occurred since the last
  }               // check


  // some socket errors have occurred since the last 
  // check
  

  // go over all failed transactions and get them from
  // the map  
  decltype(responses_.begin()) mapIter;

  for(auto failID : sockErrors)
  {
    mapIter = responses_.find(failID);
    
    if(mapIter == responses_.end())
    {
      // need to log this case
      LOG(WARNING) << "Socket Failure Message does not have a valid tranID";
    }
    
   else
   {
     // handle the failure
     handleInternalError(mapIter->second, CommonCode::IOStatus::ERR_INTERNAL);
     responses_.erase(mapIter);

     // mark the transaction ID as completed
     completedTransaction(failID);
   } // else
    
  } // for
 

  sockErrors.clear(); // clear explicitly
  // signal that some transactions have been handled
  return true; 
  
}


void
SecurityModule::dequeueSocketErrors(std::vector<uint32_t>& errors)
{
  std::lock_guard<std::mutex> tmpLock(errLock_);

  // dequeue all socket errors
  if(sockErrs_.empty())
  {
    return; // no socket write errors
  }

  // dequeue the errors
  errors.reserve(sockErrs_.size());

  for(const auto tID : sockErrs_)
  {
    errors.push_back(tID);
  }

  // clear the errors
  sockErrs_.clear();
 
}


void
SecurityModule::doStartService()
{
  std::lock_guard<std::mutex> tmpLock(termLock_);

  std::thread tmpThread(&SecurityModule::processRequests, this);
  // move the reference to the class attribute
  workerThread_ = std::move(tmpThread);
  active_ = true; 
}



void
SecurityModule::signalTerminate()
{

  // create a tmp signal
  SecurityModule::TaskWrapper tmpWrap;
  tmpWrap.resType_ = SecurityModule::MessageSource::MSG_TERM;
  // enqueue the signal
  tasks_.push(std::move(tmpWrap));

}



void
SecurityModule::joinService()
{


  // send a signal to the security moduel
  // to terminate
  signalTerminate();
  
  workerThread_.join(); // wait the worker thread to
                        // terminate

}


void
SecurityModule::destroy()
{
  // make sure the service has been stopped
  const auto flag = Security::done_.load(std::memory_order_acquire);
  
  if(!flag)
  {
    // stop the service
    Security::stopService();  
  }
 

  // wait until the worker thread is done
  { 
    std::unique_lock<std::mutex> tmpLock(termLock_);
   
    while(active_)
    {
      termVar_.wait(tmpLock);
    }

  } // lock scope 

  // done (now it is safe to destroy the object)

}

} // namespace singaistorageipc
