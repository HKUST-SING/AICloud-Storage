#pragma once


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
 



    /**
     * Encapsulates multiple types of the responses
     */
    struct TaskWrapper
    {
     Request msg_;

     typedef union Response
     {
       folly::Promise<Task>        task_;
       folly::Promise<IOResponse>  ioRes_;
     } Response; 
    }; // struct TaskWrapper     


    public:
      SecurityModule(std::unique_ptr<ServerChannel>&& comm)
      : Security(std::move(comm))
      {}
       
 
      ~SecurityModule override;
       
       virtual folly::Future<Task>
           clientConnect(const UserAuth& user) override;


       virtual folly::Future<Response>
         checkPerm(const std::string& path,
                   const UserAuth& user,
                   const Task::OpType op,
                   const uint64_t dataSize = 0) override;


      virtual folly::Future<ObjectInfo>
        lockandRetrieveInfo(const std::string& path,
                            const Task::OpType op)
        {
          ObjectInfo tmp;
          return folly::Future<ObjectInfo>(std::move(tmp));
        } 


     virtual folly::Future<Response>
       sendIOResult(const IOResult& rest) override;





    private:
      ConcurrentQueue<Message> tasks_; // queue of messages to send 
      std::map<uint32_t, > responses_; // reponse futures


      uint32_t nextID_; // next transaction id
      uint32_t headID_; // latest transaction in fly
   
       

  }; // class SecurityModule


} // namespace singaistorageipc
