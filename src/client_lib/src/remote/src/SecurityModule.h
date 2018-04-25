#pragma once

// C++ std lib
#include <map>
#include <deque>
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>


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
       std::shared_ptr<Request>    msg_;     // message
       MessageSource               resType_; // response to use
       
       union TaskPtrs
       {
         folly::Promise<Task>*       serverTask_;  //  server task
         folly::Promise<IOResponse>* workerTask_;  // worker task
       } futRes; // union

       TaskWrapper() 
       : msg_{nullptr},
         resType_(MessageSource::MSG_EMPTY)
       {
         futRes.serverTask_ = nullptr;
         futRes.workerTask_ = nullptr;
       }

       TaskWrapper(const struct TaskWrapper&) = delete;
       TaskWrapper& operator=(const struct TaskWrapper&) = delete;
      
 
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
           default:
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

       TaskWrapper& operator=(struct TaskWrapper&& other)
       {

         if(this != &other)
         {
    
           msg_     = std::move(other.msg_);
           resType_ = other.resType_;

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
             default:
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

         } //if

         return *this; // return the same object
       }

    
       // delete the active future
       ~TaskWrapper()
        {
          // destroy the message
          msg_ = nullptr;

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
            default:
            {
              break;
            }    
          } // switch
  
          // set the pointer to nullptr
          futRes.serverTask_ = nullptr;
          futRes.workerTask_ = nullptr;
        }
          
      } TaskWrapper; // struct TaskWrapper     


      // Callback used by asyn socket for notigying about read/write
      class FollySocketCallback : public folly::AsyncWriter::WriteCallback
      {
      
        public:
          FollySocketCallback() = delete;
          
          FollySocketCallback(SecurityModule* const sec,
                              const uint32_t tranID);

          ~FollySocketCallback() = default;

          inline void writeSuccess() noexcept override;

    
          inline void writeErr(const size_t bytesWritten, 
                        const folly::AsyncSocketException& exp)
                            noexcept override;
            

          private:
            SecurityModule* const secMod_; // security module
            const uint32_t        tranID_; // transaction ID (mesage id)

      }; // class FollySocketCallback


    public:
      SecurityModule() = delete; // no default constructor
      SecurityModule(const SecurityModule&) = delete;
      SecurityModule& operator=(const SecurityModule&) = delete; 
      SecurityModule(SecurityModule&&) = delete;
      SecurityModule& operator=(SecurityModule&&) = delete;
  

      // supported constructors
      SecurityModule(std::unique_ptr<ServerChannel>&& comm);


      bool initialize() override
      {
        return channel_->initChannel();
      }      

 
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


       
       /** 
        * It is possible that the write operation 
        * on the socket failed; need to notify
        * the taks issuer about it (INTERNAL ERROR).
        */
       void socketError(const uint32_t errTranID);



    protected:
      /**
       * The Security class keeps the destructor protected,
       * maintain the level of visibility.
       */
      ~SecurityModule() override;


       /**
        * For ensuring that the object is destroyed in
        * a safe way (no problems with the destructor).
        * (This method is supposed to be used to avoid
        * dagling pointers in the class).
        */
       void destroy() override;

      /**
       * The calling thread waits the security module
       * to terminate/finish processing.
       */
      virtual void joinService() override;


      /**
       * Start the worker thread for this module.
       */
      virtual void doStartService() override;



    private: // private utility methods
    
      /** 
       * Thread method for running the security service
       * on a separate thread
       */
      void processRequests(); 


      /**
       * Get the reponse from the server channel.
       * It may not return any new messages if there
       * nothing to poll.
       */ 
      void queryServerChannel();

      
      /**
       * Process newly dequeued requests/tasks.
       */
      void processNewTasks();


      /** 
       * Dequeue any failed socket operations.
       * Method is called to check for writeErr callback() [Folly]
       **/
      void dequeueSocketErrors(std::vector<uint32_t>& errors);


      /** 
       * There is a possibility of having socket errors.
       * Socket errors are handled with a low priority.
       * (NOTE:  give higher priority in the future)
       *
       * @return : true if any socket error have been handled
       */

      bool tryHandleSocketErrors();


      /**
       * Update internal window so that the ID could
       * keep growing.
       *
       * @param: tranID: id of a completed transaction
       */ 
      inline void completedTransaction(const uint32_t tranID);



      /** 
       * Basically, handles the socket related errors.
       * Notifies the caller about an internal error.
       */
      void handleInternalError(TaskWrapper& errTask, 
       const CommonCode::IOStatus errNo = CommonCode::IOStatus::ERR_INTERNAL) const;



      /**
       * Process a recieved request and send it to the IPC server.
       *
       * @param: taskRef : reference to the task wrapper that
       *                   issued a request for the server
       * @param: resValue : response from the server
       */
       void processServerResponse(TaskWrapper& taskRef, 
                                  std::unique_ptr<Response>&& resValue);

      /**
       * Process a recieved request and send it to one of the workers.
       *
       * @param: taskRef : reference to the task wrapper that
       *                   issued a request for the server
       * @param: resValue : response from the server
       */
       void processWorkerResponse(TaskWrapper& taskRef, 
                                  std::unique_ptr<Response>&& resValue);



      /** 
       * Stop the module and clean up all tasks.
       */
       void closeSecurityModule();



    private:

      std::thread                     workerThread_; // worker thread
      ConcurrentQueue<TaskWrapper, std::deque<TaskWrapper>> tasks_; // queue of messages to send 
      std::map<uint32_t, TaskWrapper> responses_; // futures for respones
      std::deque<TaskWrapper>         recvTasks_; // for dequeueing
        
 
      std::map<uint32_t, bool>        complTrans_; // completed transactions      
      std::deque<TaskWrapper>         pendTrans_;  // should never happen                                         
      

      /************** For locally failed write operations *************/
      std::mutex                      errLock_;  // error lock
      std::deque<uint32_t>            sockErrs_; // failed write socket
                                                 // messages

      uint32_t                        nextID_; // next transaction ID
      uint32_t                        backID_; // oldest transaction ID 

      std::mutex                      termLock_; // for clean 
                                                 // termination

      std::condition_variable         termVar_;  // for clean term
      bool                            active_;   // active module                       


  }; // class SecurityModule


} // namespace singaistorageipc
