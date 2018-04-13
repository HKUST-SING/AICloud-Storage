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


    std::shared_ptr<RemoteProtocol::ProtocolHandler>
         handleMessage(std::shared_ptr<IOresponse> ioRes,
                       const Task& task,
                       CephContext& cephCtx) override;



  protected:
    bool doInitialize() override {return true;} 

   
    void doClose() override {}



  private:
    JSONHandler<const uint8_t*> jsonDecoder_; // json decoder
    
    


}; // class JSONRemoteProtocol


} // namespace singaistorageipc
