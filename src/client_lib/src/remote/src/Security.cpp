// C++ std lib 

// Project lib
#include "remote/Security.h"
#include "SecurityModule.h"


namespace singaistorageipc
{

std::shared_ptr<Security>
Security::createSecurityModule(const char* secType,
     std::unique_ptr<ServerChannel>&& channel,
     std::unique_ptr<SecureKey>&&     secKey,
     std::unique_ptr<Cache>&&         cache)
{
  // now only one type is supported
  std::make_shared<SecurityModule>(std::move(channel));

}

} // namespace singaistorageipc
