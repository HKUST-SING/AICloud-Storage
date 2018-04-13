
// C++ std lib
#include <cstring>
#include <memory>


// Project lib
#include "cluster/RemoteProtocol.h"
#include "JSONRemoteProtocol.h"


namespace singaistorageipc
{

// initialize the static value
const std::string RemoteProtocol::ProtocolHandler::empty_value("");



std::unique_ptr<RemoteProtocol>
RemoteProtocol::createRemoteProtocol(const char* protType)
{

  // only JSON supported now
  return std::make_unique<JSONRemoteProtocol>();
  
}



} // namespace singaistorageipc
