#pragma once

// C++ std lib
#include <map>
#include <queue>


// Project lib
#include "remote/Security.h"
#include "include/ConcurrentQueue.h"
#include "include/Task.h"

namespace singaistorageipc
{

  class SecurityModule : public Security
  {
    /**
     * Implementation of the security module.
     * This module is pretty raw and does not do
     * much since it is needed to test the communication
     * first. The caching and other things are disabled.
     */
 
    private:

      enum class MessageSource : int
      {
        MSG_SERVER = 1, 
        MSG_WORKER = 2,
        MSG_EMPTY  = 255 // no message
      }; // who sent the request - server of worker


      /**
       * Encapsulates multiple types of the responses
       */

      typedef struct TaskWrapper
      {
       Request                     msg_;     // message
       MessageSource               resType_; // response to use
       
       union TaskPtrs
       {
         folly::Promise<Task>*       serverTask_;  //  server task
         folly::Promise<IOResponse>* workerTask_;  // worker task
       } futRes; // union

       TaskWrapper() 
       : msg_(0, std::string(""), std::string(""), 
                 std::string(""), CommonCode::IOOpCode::OP_NOP),
         resType_(MessageSource::MSG_EMPTY),
         futRes()
       {
         futRes.serverTask_ = nullptr;
         futRes.workerTask_ = nullptr;
       }

       TaskWrapper(const struct TaskWrapper&) = delete;
       
       TaskWrapper(struct TaskWrapper&& other)
       : msg_(std::move(other.msg_)),
         resType_(other.resType_)
       {
         switch(other.resType_)
         {
           case MessageSource::MSG_SERVER:
           {
             futRes.workerTask_ = nullptr;
             futRes.serverTask_ = other.futRes.serverTask_;
             break;
           }
           case MessageSource::MSG_WORKER:
           {
             futRes.serverTask_ = nullptr;
             futRes.workerTask_ = other.futRes.workerTask_;
             break;
           }
           defualt:
           {
             futRes.serverTask_ = nullptr;
             futRes.workerTask_ = nullptr;
             break;
           }    
         } // switch

         // reset the pointers of the other
         other.futRes.serverTask_ = nullptr;
         other.futRes.workerTask_ = nullptr;
         other.resType_ = MessageSource::MSG_EMPTY;
       }
    
       // delete the active future
       ~TaskWrapper()
        {
          switch(resType_)
          {
            case MessageSource::MSG_SERVER:
            {
             delete futRes.serverTask_;
             break;
            }
            case MessageSource::MSG_WORKER:
            {
              delete futRes.workerTask_;
              break;
            }
            defualt:
            {
              break;
            }    
          } // switch
  
          // set the pointer to nullptr
          futRes.serverTask_ = nullptr;
          futRes.workerTask_ = nullptr;
        }
          
      } TaskWrapper; // struct TaskWrapper     


    public:
      SecurityModule(std::unique_ptr<ServerChannel>&& comm)
      : Security(std::move(comm))
      {}
       
 
      ~SecurityModule() override;
       
       virtual folly::Future<Task>
           clientConnect(const UserAuth& user) override;


       virtual folly::Future<IOResponse>
         checkPerm(const std::string& path,
                   const UserAuth& user,
                   const CommonCode::IOOpCode op,
                   const uint64_t dataSize = 0) override;


      /*virtual folly::Future<ObjectInfo>
        lockandRetrieveInfo(const std::string& path,
                            const Task::OpType op)
        {
          ObjectInfo tmp;
          return folly::Future<ObjectInfo>(std::move(tmp));
        }*/ 


     virtual folly::Future<IOResponse>
       sendIOResult( const std::string& path,
                     const UserAuth& user,
                     const CommonCode::IOOpCode op,
                     const IOResult& res) override;


      virtual folly::Future<IOResponse>
       sendIOResult( std::string&& path,
                     UserAuth&& user,
                     const CommonCode::IOOpCode op,
                     IOResult&& res) override;


    private:
      ConcurrentQueue<TaskWrapper>    tasks_; // queue of messages to send 
      std::map<uint32_t, TaskWrapper> responses_; // reponse futures
           
 
      std::map<uint32_t, bool>        complTrans_; // completed transactions      
      std::queue<TaskWrapper>         pendTrans_; // should never happen                                         
      
      uint32_t nextID_; // next transaction ID
      uint32_t backID_; // oldest transaction ID 

  }; // class SecurityModule


} // namespace singaistorageipc
