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


      using sing_err_t = const uint64_t;

    public:

      /* Internal SING ERROR Codes */
      static sing_err_t INTERNAL_ERR   = 255;
      static sing_err_t SUCCESS        =   0;
      static sing_err_t USER_ERR       =   1; // no such user
      static sing_err_t PASSWD_ERR     =   2; // wrong password
      static sing_err_t PATH_NOT_FOUND =   3; // path not found
      static sing_err_t ACL_ERR        =   4; // ACL error
      static sing_err_t QUOTA_ERR      =   5; // quota error
      static sing_err_t LARGE_ERR      =   6; // object too big 
      static sing_err_t SMALL_ERR      =   7; // object too small
      static sing_err_t BODY_TYPE_ERR  =   8; // wrong content type
      static sing_err_t BAD_PARAMS     =   9; // wrong op parameters

  }; // class SingErrorCode

} // namespace singsotrage
} // namespace rgw

#endif /* CEPH_RGW_SINGSTORAGE_ERROR_H */
