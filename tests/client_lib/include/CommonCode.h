#pragma once

namespace singaistorageipc
{

typedef struct CommonCode final
{

  enum class IOOpCode : int
  {
    OP_READ         = 1,
    OP_WRITE        = 2,
    OP_DELETE       = 3,
    OP_AUTH         = 4,
    OP_COMMIT       = 5,
    OP_CLOSE        = 6,
    OP_APPEND       = 7,  // same as write only that append data
    OP_CHECK_WRITE  = 8,  // check if can write that many bytes
    OP_ABORT        = 9,  // abort active IO operation
    OP_EXIT         = 10, // exit the processing step
    OP_NOP          = 255 // no operation (for default initialization)

  }; // enum IOOpCode


  enum class IOStatus : uint8_t
  {
    STAT_SUCCESS       = 0,  // IO operation successfully completed
    ERR_USER           = 1,  // wrong username
    ERR_PASS           = 2,  // wrong password
    ERR_PATH           = 3,  // wrong obejct path
    ERR_DENY           = 4,  // deny access to data at 'path'
    ERR_QUOTA          = 5,  // cannot write more (quota exceeded)
    ERR_OBJ_LARGE      = 6,  // object too big to write
    ERR_OBJ_SMALL      = 7,  // object too small to write
    ERR_CONTENT        = 8,  // message conntent cannot be understood
    ERR_PARAMS         = 9,  // parameters cannot be understood
    ERR_PROT           = 10, // wrong protocol used
    ERR_LOCK           = 11, // cannot acquire lock on data
    STAT_PARTIAL_READ  = 12, // there is more data to read
    STAT_PARTIAL_WRITE = 13, // there is merging write
    STAT_CLOSE         = 14, // close op succeeded    

    ERR_INTERNAL      = 255 // internal error
  }; // enum IOStatus



} ComonCode; // struct CommonCode
} // namesapce singaistorageipc
