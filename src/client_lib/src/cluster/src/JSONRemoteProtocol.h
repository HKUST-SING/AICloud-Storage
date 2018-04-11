#pragma once

// C++ std lib


// Project lib
#include "cluster/RemoteProtocol.h"
#include "include/JSONHandler.h"

namespace singaistorageipc
{


class JSONRemoteProtocol : public RemoteProtocol
{

  /**
   * This class provides an implementation to the 'RemoteProtocol'
   * interface and uses JSON as its underlying communication encoding.
   * The protocol tries to convert received JSON messages into
   * a RadosContext which can be used by a worker to perform RADOS
   * IO operations.
   */

  public:
    JSONRemoteProtocol() = default;


    void handleMessage(std::shared_ptr<IOresponse> ioRes,
                       const RemoteProtocol::ProtocolCallback& protCall,
                       RemoteProtocol::CallbackContext context) override

    {

    }



  protected:
    bool doInitializeProtocol() override
    {
      return true;
    }

   
    void doStopProtocol() override
    {

    }



  private:
    JSONHandler<const uint8_t*> jsonDecoder_; // json decoder
    


}; // class JSONRemoteProtocol


} // namespace singaistorageipc
