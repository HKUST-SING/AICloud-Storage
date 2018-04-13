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


  private:
    class JSONProtocolReadHandler : public RemoteProtocol::ProtocolHandler
    {
      public:
        JSONProtocolReadHandler(CephContext& cephCont);


        ~JSONProtocolReadHandler() override;



         void handleJSONMessage(JSONDecoder<const uint8_t*>& dec, 
                                std::shared_ptr<IOResponse> ioRes,
                                const Task& task);
       


         uint64_t writeData(const char*    rawData,
                            const uint64_t writeBytes,
                            void*          userCtx) override

         {
           return 0;  /*Do not support this*/
         }

         uint64_t readData(const uint64_t readBytes,
                           void*          userCtx) override;


         uint64_t getTotalDataSize() const override;
        

    }; // class  JSONProtocolReadHandler


    class JSONProtocolWriteHandler : public RemoteProtocol::ProtocolHandler
    {
      public:
        JSONProtocolWriteHandler(CephContext& cephCont);
       

        ~JSONProtocolWriteHandler() override;


        bool needNotifyCephSuccess() const override
        {
          return true;
        }
       

        std::unique_ptr<IOResult> getCephSuccessResult() override;



        void handleJSONMessage(JSONDecoder<const uint8_t*>& dec, 
                               std::shared_ptr<IOResponse> ioRes,
                               const Task& task);


        uint64_t writeData(const char*    rawData,
                           const uint64_t writeBytes,
                           void*          userCtx) override;


        uint64_t readData(const uint64_t readBytes,
                          void*          userCtx) override

        { 
          return 0; 

        } /* Do not support this */


      private:
        std::unique_ptr<IOResult> successRes_;
 
        

    }; // class  JSONProtocolWriteHandler


    class JSONProtocolAuthHandler : public RemoteProtocol::ProtocolHandler
    {
      public:
        JSONProtocolAuthHandler(CephContext& cephCont);


         ~JSONProtocolAuthHandler() override;


         uint64_t writeData(const char*    rawData,
                            const uint64_t writeBytes,
                            void*          userCtx) override

          {
            return 0;
          }


          void handleJSONMessage(JSONDecoder<const uint8_t*>& dec, 
                                std::shared_ptr<IOResponse> ioRes,
                                const Task& task);


         uint64_t readData(const uint64_t readBytes,
                           void*          userCtx) override

         { 
           return 0; 

         } /* Do not support this */

         
         // Implement key/value store for retrieving some
         // metadata
         const std::string& 
             getValue(const std::string& key) const override;



         const std::string& 
             getValue(const char* key) const override;
         

      private:
        std::map<std::string, std::string> metadata_;
 
        

    }; // class  JSONProtocolAuthHandler



    class JSONProtocolDeleteHandler : public RemoteProtocol::ProtocolHandler
    {
      public:
        JSONProtocolDeleteHandler(CephContext& cephCont);


        ~JSONProtocolDeleteHandler() override;


        void handleJSONMessage(JSONDecoder<const uint8_t*>& dec, 
                               std::shared_ptr<IOResponse> ioRes,
                               const Task& task);

        uint64_t writeData(const char*    rawData,
                           const uint64_t writeBytes,
                           void*          userCtx) override

        {
          return 0;
        }


        uint64_t readData(const uint64_t readBytes,
                          void*          userCtx) override
        { 
          return 0; 

        } /* Do not support this */
 
        

    }; // class  JSONProtocolDeleteHandler


    
    /**
     * This handler is 'void' because it is returned when there is
     * any error related not to JSON (wrong protocol and so on).
     */
    class JSONProtocolVoidHandler : public RemoteProtocol::ProtocolHandler
    {
      public:
        JSONProtocolVoidHandler(CephContext& cephCont,
                                const CommonCode::IOOpCode opCode);


         ~JSONProtocolVoidHandler() override = default;


         uint64_t writeData(const char*    rawData,
                            const uint64_t writeBytes,
                            void*          userCtx) override

         {
           return 0;
         }


         uint64_t readData(const uint64_t readBytes,
                           void*          userCtx) override

         { 
           return 0; 

          } /* Do not support this */
 
        
         

    }; // class  JSONProtocolErrHandler
  
    


  public:
    JSONRemoteProtocol();


    std::shared_ptr<RemoteProtocol::ProtocolHandler>
         handleMessage(std::shared_ptr<IOResponse> ioRes,
                       const Task& task,
                       CephContext& cephCtx) override;


    ~JSONRemoteProtocol() override;



  protected:
    bool doInitialize() override;

   
    void doClose() override;



  private:
    JSONDecoder<const uint8_t*> jsonDecoder_; // json decoder
    
    


}; // class JSONRemoteProtocol


} // namespace singaistorageipc
