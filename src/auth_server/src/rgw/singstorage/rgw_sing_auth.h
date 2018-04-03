#ifndef CEPH_RGW_SINGSTORAGE_AUTH_H
#define CEPH_RGW_SINGSTORAGE_AUTH_H


#include "../rgw_op.h"
#include "../rgw_rest.h"
#include "../rgw_auth.h"
#include "../rgw_auth_filters.h"
#include <utility>



/**
 * Class uses a very similar way of authenticating storage users
 * to the one used by the provided SWIFT implementation. In fact,
 * the system uses a lot of the swift code to authenticate users.
 */



namespace rgw {
namespace auth {
namespace singstorage { 

class SignedMachineEngine : public rgw::auth::Engine

{
  /**
   * The class implements a simple authentication engine. If an
   * HTTP request has a shared secret key in its header, 
   * it means that the passed user has been authenticated
   * with a password and it is possible to proceed further.
   */

  private:
    using result_t = rgw::auth::Engine::result_t;
    static const constexpr std::string::size_type AUTH_KEY_LENGTH = 
                           std::string::size_type(256);
    
    CephContext* const cct_;
    RGWRados* const store_;
    const rgw::auth::TokenExtractor* const extractor_;
    const rgw::auth::LocalApplier::Factory* const apl_factory_;

   
    bool is_applicable(const std::string& token) const noexcept;
    result_t authenticate(const std::string& token, 
                          const req_state*s ) const;


    std::pair<std::string, std::string> extract_auth_data(
                           const std::string& token) const;

    bool valid_shared_key(const std::string& secret) const;



  public:
    SignedMachineEngine(CephContext* const cct,
                        RGWRados* const store,
                        const rgw::auth::TokenExtractor* const extractor,
                        const rgw::auth::LocalApplier::Factory* const apl_factory)

    : cct_(cct),
      store_(store),
      extractor_(extractor),
      apl_factory_(apl_factory)
      {}

    const char* get_name() const noexcept override {
    
      return "rgw::auth::singstorage::SignedMachineEngine";
    }


    /* public interface used by the processors */
    result_t authenticate(const req_state* const st) const override {
     return authenticate(extractor_->get_token(st), st);
   }


}; // class SignedMachineEngine 



class DefaultStrategy : public rgw::auth::Strategy,
                        public rgw::auth::LocalApplier::Factory
{

  /** 
   *  A simpler version of the rgw::auth::swift::DefaultStrategy.
   *  We don't use RemoteAppliers and also we don't inherit from
   *  the rgw::auth::TokenExtractor - we prefer composition.
   */


  private:
 
   class SimpleTokenExtractor : public rgw::auth::TokenExtractor
   {
     /** 
      * Class Implements the TokenExtractor interface
      * from the rgw_auth.h file.
      */

     public:
       SimpleTokenExtractor()
       : rgw::auth::TokenExtractor()
        {}

       ~SimpleTokenExtractor() {}

       virtual std::string get_token(const req_state* state) const
       {
         return state->info.env->get("HTTP_X_AUTH_TOKEN", "");
       }
     
   }; // class SimpleTokenExtractor

  SimpleTokenExtractor* token_extr_;
  RGWRados* const store_; // Rados Engine
  using aplptr_t = rgw::auth::IdentityApplier::aplptr_t;
  const rgw::auth::singstorage::SignedMachineEngine machine_engine_;


  /** 
   * Only one apl is supported (local one)
   */

  aplptr_t create_apl_local(CephContext* const cct,
                            const req_state* const s,
                            const RGWUserInfo& user_info,
                            const std::string& subuser) const override


  {
    auto apl = rgw::auth::add_3rdparty(store_, s->account_name,
                    rgw::auth::add_sysreq(cct, store_, s,
                         rgw::auth::LocalApplier(cct, user_info, subuser)));

    return aplptr_t(new decltype(apl)(std::move(apl)));
  }


  public:
    DefaultStrategy(CephContext* const cct, 
                    RGWRados* const store)

    : token_extr_(new SimpleTokenExtractor()),
      store_(store),
      machine_engine_(cct, store, 
             static_cast<rgw::auth::TokenExtractor*>(token_extr_),
             static_cast<rgw::auth::LocalApplier::Factory*>(this))

     {
       assert(token_extr_); /// make sure there was no memory problem
       
       using Control = rgw::auth::Strategy::Control;

       // since all the engines have been initialized in the 
       // initializcation list, we can safely add them
       add_engine(Control::REQUISITE, machine_engine_);
     }


    const char* get_name() const noexcept override
    {
      return "rgw::auth::singstorage::DefaultStrategy";
    }



   ~DefaultStrategy()
    {
      // delete the token extractor
      delete token_extr_;
      token_extr_ = nullptr;
    }      



}; // class DefaultStrategy




} /* namesapce singsotrage */
} /* namespace auth */
} /* namespace rgw */


class RGW_SINGSTORAGE_Auth_Get: public RGWOp {
public:
  RGW_SINGSTORAGE_Auth_Get() {}
  ~RGW_SINGSTORAGE_Auth_Get() override {}


  int verify_permission() override { return 0; }
  void execute() override;
  const string name() override { return "singstorage_auth_get"; }
}; // class RGW_SINGSTORAGE_Auth_Get


class RGWHandler_SINGSTORAGE_Auth: public RGWHandler_REST {
public:
  RGWHandler_SINGSTORAGE_Auth() {}
  ~RGWHandler_SINGSTORAGE_Auth() override {}
  RGWOp* op_get() override;


  int init(RGWRados* store, struct req_state* state, rgw::io::BasicClient* cio) override;
  int authorize() override { return 0; }
  int postauth_init() override { return 0; }
  int read_permissions(RGWOp* op) override { return 0; }

  virtual RGWAccessControlPolicy* alloc_policy() {return nullptr;}
  virtual void free_policy(RGWAccessControlPolicy* policy) {}

}; // class RGWHandler_SINGSTORAGE_Auth



class RGWRESTMgr_SINGSTORAGE_Auth: public RGWRESTMgr {
public:
  RGWRESTMgr_SINGSTORAGE_Auth() = default;
  ~RGWRESTMgr_SINGSTORAGE_Auth()  override = default;

  RGWRESTMgr* get_resource_mgr(struct req_state* const s,
                               const std::string& uri,
                               std::string* const out_uri) override {

    return this;
  }


  RGWHandler_REST* get_handler(struct req_state*,
                               const rgw::auth::StrategyRegistry&,
                               const std::string&) override {

    return new RGWHandler_SINGSTORAGE_Auth;
  }


}; // class RGWRESTMgr_SINGSOTRAGE_Auth


#endif /* CEPH_RGW_SINGSTORAGE_AUTH_H */
