#pragma once

// C++ std lib
#include <memory>
#include <functional>


// Project lib
#include "cluster/CephContext.h"
#include "cluster/Worker.h"
#include "include/Task.h"

namespace singaistorageipc
{

class RemoteProtocol
{

  /**
   * The class is an interface for all protocols
   * used between a worker and a remote authentication/metadata
   * server. The implementations should take an IOResponse
   * as an input and provide capabilities for performing
   * Rados Cluster Operations.
   */

  public:

    typedef struct Result
    {
      CommonCode::IOOpCode opCode;
      CommonCode::IOStatus opStatus;

      IOResult*            success; // now we only have responses on
                                    // success

      /**
       * A couple of constructors and assignment operations.
       */

      Result(IOResult* resMsg = nullptr,
             const CommonCode::IOOpCode op = 
                   CommonCode::IOOpCode::OP_NOP,
             const CommonCode::IOStatus stat = 
                   CommonCode::IOStatus::ERR_INTERNAL)
      : opCode(op),
        opStatus(stat),
        success(resMsg)  
        {}

     /**
      * Move constructor.
      */
     Result(struct RemoteProtocol::Result&& other)
     : opCode(other.opCode),
       opStatus(other.opStatus),
       success(other.success)
     {
       //reset the other to nullptr
       other.success = nullptr;
     }

    /**
     * Move assignment.
     */
    Result& operator=(struct RemoteProtocol::Result&& other)
    {
      if(this != &other)
      {
        // delete current message and set to nullptr
        if(this->success)
        {
          delete this->success;
        }

        this->opCode   = other.opCode;
        this->opStatus = other.opStatus; 
        this->success  = other.success;

        // set the other to nullptr
        other.success = nullptr;
       
      }

      return *this;
    }


    /**
     * Destructor (may be virtual in the future since Result shall
     * be used by the derived classes).
     */

    ~Result()
     {
       if(success)
       {
         delete success;
         success = nullptr;
       }
     }


    } Result; // protocol result (success, failure, or something)


    using CallbackContext  = void*; // callback context
    using ProtocolCallback = std::function<void(
                               std::unique_ptr<RemoteProtocol::Result>&&,
                               CallbackContext)>;



  public:
    RemoteProtocol() = default;
    RemoteProtocol(const RemoteProtocol&) = default;
    RemoteProtocol& operator=(const RemoteProtocol&) = default;
    RemoteProtocol& operator=(RemoteProtocol&&)      = default;

    /**
     * Move constructor can be supported
     */
     RemoteProtocol(RemoteProtocol&& other) = default;


    virtual ~RemoteProtocol() = default; // shall be virtual


    bool initializeProtocol()
    {
      return doInitializeProtocol();
    }


    void stopProtocol()
    {  
      doStopProtocol(); // derived implementation
    }



    virtual void handleMessage(std::shared_ptr<IOResponse> ioRes, 
                               const ProtocolCallback& callBack,
                               CallbackContext context) = 0;




  protected:
    /**
     * Derived classes use this method to initialize themselves.
     */
    virtual bool doInitializeProtocol() = 0;


    /**
     * derived classes use this method to terminate themselves.
     */
    virtual void doStopProtocol() = 0;

}; // class RemoteProtocol


} // namespace singaistorageipc
