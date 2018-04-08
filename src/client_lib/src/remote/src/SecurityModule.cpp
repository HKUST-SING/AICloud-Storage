

// C++ std lib
#include <limits>

// Project lib
#include "SecurityModule.h"


using folly::Future;
using folly::Promise;


namespace singaistorageipc
{

static constexpr const uint32_t MAX_U32_VALUE = std::numeric_limits<uint32_t>::max(); // size of uint32_t

static constexpr const uint32_t MAXIMUM_WINDOW_SIZE = (std::numeric_limits<uint32_t>::max() >> 1); // maximum gap between nextID_ and headID_


// helper function for determining if the security module can  issue
// another request to the channel
static uint32_t getWindowSize(const uint32_t headSeq, 
                              const uint32_t backSeq)
{
  
  // check which case has to be considered
  if(headSeq > backSeq)
  {
    return (MAX_U32_VALUE -  headSeq + backSeq);
  }
  else
  {
    return (backSeq - headSeq);
  }

}


SecurityModule::~SecurityModule()
{
  // make sure that no pending task are left
  

}


Future<Task>
SecurityModule::clientConnect(const UserAuth& user)
{

  Request authMsg(0, user.username_, user.passwd_,
          std::string(""), CommonCode::IOOpCode::OP_AUTH);

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
 
  Request dataMsg(0, user.username_, user.passwd_,
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

  Promise<IOResponse> prom;
  
  return std::move(prom.getFuture());
}


Future<IOResponse>
SecurityModule::sendIOResult(std::string&& dataPath,
                             UserAuth&&    user,
                             const CommonCode::IOOpCode op,
                             IOResult&&  res)
{

  Promise<IOResponse>* prom = new Promise<IOResponse>();

  auto futRes = prom->getFuture();
  
  TaskWrapper tmpWrap;
  tmpWrap.msg_ = std::move(res.msg_);
  tmpWrap.resType_ = SecurityModule::MessageSource::MSG_WORKER;
  tmpWrap.futRes.workerTask_ = prom;

  // enqueue the task
  tasks_.push(std::move(tmpWrap));

  // return future
  return std::move(futRes);
  
}


} // namespace singaistorageipc
