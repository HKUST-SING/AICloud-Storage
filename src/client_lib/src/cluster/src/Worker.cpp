// C++ std lib
#include <utility> 


// Project
#include "remote/Security.h" 
#include "cluster/Worker.h"
#include "StoreWorker.h"


namespace singaistorageipc
{

std::shared_ptr<Worker>
Worker::createRadosWorker(const char* type,
                    const CephContext& ctx,
                    const uint32_t id,
                    std::shared_ptr<Security> sec)
{
  // only one type is now supported
  // later might support more

  return std::make_shared<StoreWorker>(ctx, id, std::move(sec));

}



} // namespace singaistorageipc
