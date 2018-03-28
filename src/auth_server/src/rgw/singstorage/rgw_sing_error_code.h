#ifndef CEPH_RGW_SINGSTORAGE_ERROR_H
#define CEPH_RGW_SINGSTORAGE_ERROR_H

namespace rgw
{
namespace singstorage 
{
  class SINGErrorCode
  {
   /**
    * Class conatins all erorr codes used within the singstorage
    * subsystem. A separate class is introduced in order to 
    * make it easier to handle all errros and avoid
    * cyclic dependencies.
    */


    public:
   
    using sing_err_t = constexpr const uint64_t;

    /* Internal SING ERROR Codes */
    static sing_err_t INERNAL_ERR    = 255;
    static sing_err_t SUCCESS        =   0;
    static sing_err_t USER_ERR       =   1; // no such user
    static sing_err_t PASSWD_ERR     =   2; // wrong password
    static sing_err_t PATH_NOT_FOUND =   3; // path not found
    static sing_err_t ACL_ERR        =   4; // ACL error
    static sing_err_t QUOTA_ERR      =   5; // quota error 
   

  }; // class SingErrorCode

} // namespace singsotrage
} // namespace rgw

#endif /* CEPH_RGW_SINGSTORAGE_ERROR_H */
