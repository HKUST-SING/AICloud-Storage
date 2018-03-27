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

    using sing_err_t = static constexpr const uint64_t;


    public:
    /* Internal SING ERROR Codes */
    sing_err_t INERNAL_ERR    = 255;
    sing_err_t SUCCESS        =   0;
    sing_err_t USER_ERR       =   1; // no such user
    sing_err_t PASSWD_ERR     =   2; // wrong password
    sing_err_t PATH_NOT_FOUND =   3; // path not found
    sing_err_t ACL_ERR        =   4; // ACL error
    sing_err_t QUOTA_ERR      =   5; // quota error 
   

  }; // class SingErrorCode

} // namespace singsotrage
} // namespace rgw

#endif /* CEPH_RGW_SINGSTORAGE_ERROR_H */
