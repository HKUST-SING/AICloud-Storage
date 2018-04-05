


// Project
#include "remote/Security.h" 
#include "cluster/Worker.h"
#include "StoreWorker.h"


namespace singaistorageipc
{


Worker*
Worker::createRadosWorker(const std::string& type,
                    const CephContext& ctx,
                    const unsigned int id,
                    std::shared_ptr<Security> sec)
{
  // only one type is now supported
  // later might support more

  return new StoreWorker(ctx, id, sec);

}



} // namespace singaistorageipc
