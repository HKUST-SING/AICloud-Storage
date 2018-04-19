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

      private:
        typedef struct RadosObjRead
        /**
         * A Wrapper for maintaining multiple Rados Object reads.
         */
        {
          std::string poolName; // pool name
          std::string objID;    // object name

          uint64_t    size_;    // Rados object size in bytes
          uint64_t    offset_;  // where reading now this object

          uint64_t    global_;  // global offset of the value (for using
                                // algorithms)


          RadosObjRead()
          : poolName(""),
            objID(""),
            size_(0),
            offset_(0),
            global_(0)
         {}
 
         RadosObjRead(std::string&& pln,
                      std::string&& oid,
                      const uint64_t obSize = 0,
                      const uint64_t obOff = 0,
                      const uint64_t obGl = 0)
         : poolName(std::move(pln)),
           objID(std::move(oid)),
           size_(obSize),
           offset_(obOff),
           global_(obGl)
         {}


         RadosObjRead(const struct RadosObjRead& other)
         : poolName(other.poolName),
           objID(other.objID),
           size_(other.size_),
           offset_(other.offset_),
           global_(other.global_) 
         {}


         RadosObjRead(RadosObjRead&& other)
         : poolName(std::move(other.poolName)),
           objID(std::move(other.objID)),
           size_(other.size_),
           offset_(other.offset_),
           global_(other.global_)
         {}           
       
        

         ~RadosObjRead() = default;
             

        } RadosObjRead; // struct RadosObjRead

      public:
        JSONProtocolReadHandler(CephContext& cephCont);


        ~JSONProtocolReadHandler() override;



         void handleJSONMessage(JSONDecoder<const uint8_t*>& dec, 
                                std::shared_ptr<IOResponse> ioRes,
                                const Task& task);
       


         uint64_t readData(const uint64_t readBytes,
                           void*          userCtx) override;


         uint64_t getTotalDataSize() const override;

         uint64_t getDataOffset() const override;


         bool doneReading() const override;

         bool resetDataOffset(const uint64_t offset) override;


         private:
           /**
            * Process Rados Objects.
            *
            * @param: iterator which points to the 'Rados_Objs' field. 
            *
            * @return: processing code
            */
           CommonCode::IOStatus processObjects(const JSONResult::JSONIter& iter);


         private:
           uint64_t totalSize_;  // total object size 
           uint64_t readObject_; // that many bytes have read

           std::vector<RadosObjRead> radObjs_; // Rados objects
        

    }; // class  JSONProtocolReadHandler


    class JSONProtocolWriteHandler : public RemoteProtocol::ProtocolHandler
    {

      private:
        /**
         * A structure for storing all Rados Write Objects.
         */
        typedef struct RadosObjWrite
        {
          std::string poolName; // pool name
          std::string objID;    // object name
          
          uint64_t    avSize_;  // available bytes to write
          uint64_t    offset_;  // offset at which to start writing
          bool        append_;  // if append or write at offset
          
          uint64_t    global_;  // global offset of the value
                                // (for using algorithms)


          RadosObjWrite()
          : poolName(""),
            objID(""),
            avSize_(0),
            offset_(0),
            append_(true),
            global_(0)
          {}

          
          RadosObjWrite(std::string&& pln,
                       std::string&& oid,
                       const uint64_t available = 0,
                       const uint64_t wOffset   = 0,
                       const bool     needAppend = 0,
                       const uint64_t obGl       = 0)
          : poolName(std::move(pln)),
            objID(std::move(oid)),
            avSize_(available),
            offset_(wOffset),
            append_(needAppend),
            global_(obGl)
          {}

         
           RadosObjWrite(const struct RadosObjWrite& other) 
           : poolName(other.poolName),
             objID(other.objID),
             avSize_(other.avSize_),
             offset_(other.offset_),
             append_(other.append_),
             global_(other.global_)
          {}


           RadosObjWrite(struct RadosObjWrite&& other) 
           : poolName(std::move(other.poolName)),
             objID(std::move(other.objID)),
             avSize_(other.avSize_),
             offset_(other.offset_),
             append_(other.append_),
             global_(other.global_)
           {}        


        } RadosObjWrite; // struct radosObjWrite


      public:
        JSONProtocolWriteHandler(CephContext& cephCont);
       

        ~JSONProtocolWriteHandler() override;


        bool needNotifyCephSuccess() const override;
       

        std::unique_ptr<IOResult> getCephSuccessResult() override;

        bool resetDataOffset(const uint64_t offset) override;
       
        uint64_t getDataOffset() const override;
 

        void handleJSONMessage(JSONDecoder<const uint8_t*>& dec, 
                               std::shared_ptr<IOResponse> ioRes,
                               const Task& task);


        uint64_t writeData(const char*    rawData,
                           const uint64_t writeBytes,
                           void*          userCtx) override;


        uint64_t writeData(const librados::bufferlist& buffer,
                           void* userCtx) override;


      private:
        /** 
         * Private utility methods.
         */
        CommonCode::IOStatus processObjects(const JSONResult::JSONIter&);

        CommonCode::IOStatus prepareResult(std::shared_ptr<IOResponse> ioRes, const Task& task);


      private:
        std::unique_ptr<IOResult>  successRes_;
        std::vector<RadosObjWrite> radosWrites_;
        bool      avSuccess_;     // if need to send a response
        uint64_t  writeOffset_;   // current offset to write data to
 
        

    }; // class  JSONProtocolWriteHandler


    class JSONProtocolAuthHandler : public RemoteProtocol::ProtocolHandler
    {
      public:
        JSONProtocolAuthHandler(CephContext& cephCont);


         ~JSONProtocolAuthHandler() override;



          void handleJSONMessage(JSONDecoder<const uint8_t*>& dec, 
                                std::shared_ptr<IOResponse> ioRes,
                                const Task& task);


         
         // Implement key/value store for retrieving some
         // metadata
         const std::string& 
             getValue(const std::string& key) const override;



         const std::string& 
             getValue(const char* key) const override;
         

      private:
        std::map<std::string, std::string> metadata_;
 
        

    }; // class  JSONProtocolAuthHandler



    class JSONProtocolStatusHandler : public RemoteProtocol::ProtocolHandler
    {
      public:
        JSONProtocolStatusHandler(CephContext& cephCont);

        ~JSONProtocolStatusHandler() override;

         void handleJSONMessage(JSONDecoder<const uint8_t*>& dec,
                                std::shared_ptr<IOResponse> ioRes,
                                const Task& task);

    }; // class JSONProtocolStatusHandler


    class JSONProtocolDeleteHandler : public RemoteProtocol::ProtocolHandler
    {
      public:
        JSONProtocolDeleteHandler(CephContext& cephCont);


        ~JSONProtocolDeleteHandler() override;


        void handleJSONMessage(JSONDecoder<const uint8_t*>& dec, 
                               std::shared_ptr<IOResponse> ioRes,
                               const Task& task);
 
        

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
