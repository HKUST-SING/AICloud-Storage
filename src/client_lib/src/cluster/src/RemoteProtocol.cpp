
// C++ std lib
#include <cstring>
#include <memory>
#include <limits>


// Project lib
#include "cluster/RemoteProtocol.h"
#include "JSONRemoteProtocol.h"


namespace singaistorageipc
{

// initialize the static value
const std::string RemoteProtocol::ProtocolHandler::empty_value("");
const uint64_t    RemoteProtocol::ProtocolHandler::invalid_offset = std::numeric_limits<uint64_t>::max();


std::unique_ptr<RemoteProtocol>
RemoteProtocol::createRemoteProtocol(const char* protType)
{

  // only JSON supported now
  return std::make_unique<JSONRemoteProtocol>();
  
}



} // namespace singaistorageipc
