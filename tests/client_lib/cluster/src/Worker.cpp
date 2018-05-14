// C++ std lib
#include <utility> 


// Project
#include "remote/Security.h" 
#include "cluster/Worker.h"
#include "StoreWorker.h"


namespace singaistorageipc
{

std::unique_ptr<Worker>
Worker::createRadosWorker(const char* type,
                    std::unique_ptr<CephContext>&&    ctx,
                    std::unique_ptr<RemoteProtocol>&& prot,
                    const uint32_t id,
                    std::shared_ptr<Security> sec)
{
  // only one type is now supported
  // later might support more

  return std::make_unique<StoreWorker>(std::move(ctx),
                                       std::move(prot),
                                       id, sec);

}


std::unique_ptr<Worker>
Worker::createRadosWorker(const std::string& type,
                    std::unique_ptr<CephContext>&&    ctx,
                    std::unique_ptr<RemoteProtocol>&& prot,
                    const uint32_t id,
                    std::shared_ptr<Security> sec)
{
  // only one type is now supported
  // later might support more

  return std::make_unique<StoreWorker>(std::move(ctx), 
                                       std::move(prot),
                                       id, sec);

}


} // namespace singaistorageipc
