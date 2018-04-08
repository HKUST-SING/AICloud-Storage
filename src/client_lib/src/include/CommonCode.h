#pragma once

namespace singaistorageipc{

struct CommonCode final
{

  enum class IOOpCode : int
  {
    OP_READ   = 1,
    OP_WRITE  = 2,
    OP_DELETE = 3,
    OP_AUTH   = 4,
    OP_COMMIT = 5,
    OP_CLOSE  = 6 

  }; // enum IOOpCode


  enum class IOStatus : uint8_t
  {
    STAT_SUCCESS      = 0,  // IO operation successfully completed
    STAT_CLOSE        = 1,  // close op succeeded
    ERR_USER          = 2,  // wrong username
    ERR_PASS          = 3,  // wrong password
    ERR_PATH          = 4,  // wrong obejct path
    ERR_DENY          = 5,  // deny access to data at 'path'
    ERR_QUOTA         = 6,  // cannot write more (quota exceeded)
    ERR_PROT          = 7,  // wrong protocol used
    ERR_LOCK          = 8,  // cannot acquire lock on data
    STAT_PARTIAL_READ = 9,  // there is more data to read

    ERR_INTERNAL      = 255 // internal error
  }; // enum IOStatus



}; // struct CommonCode

} // namespace singaistorageipc