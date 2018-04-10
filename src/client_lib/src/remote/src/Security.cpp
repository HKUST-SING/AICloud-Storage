// C++ std lib 

// Project lib
#include "remote/Security.h"
#include "SecurityModule.h"


namespace singaistorageipc
{


Security::Security(Security&& other)
: channel_(std::move(other.channel_)),
  secret_(std::move(other.secret_)),
  cache_(std::move(other.cache_))
{}

Security&
Security::operator=(Security&& other)
{

  if(this != &other)
  {
    channel_ = std::move(other.channel_);
    secret_  = std::move(other.secret_);
    cache_   = std::move(other.cache_);
  }

  return *this;
}


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
