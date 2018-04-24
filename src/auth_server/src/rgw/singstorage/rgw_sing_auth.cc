/**
 * Class uses a very similar way of authenticating storage users
 * to the one used by the provided SWIFT implementation. In fact,
 * the system uses a lot of the swift code to authenticate users.
 */

#include <cstring>

#include "../rgw_client_io.h"
#include "../rgw_http_client.h"
#include "../rgw_rest.h"
#include "../rgw_common.h"


#include "rgw_sing_auth.h"
#include "rgw_sing_error_code.h"



#define DEFAULT_SINGSTORAGE_PREFIX "/sing"
#define AUTH_USER 0x01 // no such user found
#define AUTH_PASS 0x02 // password is incorrect
#define AUTH_ERR  0x10 // some internal error


namespace rgw {
namespace auth {
namespace singstorage { 



static std::string extract_swift_subuser_sing(const std::string& swift_user_name) {
  size_t pos = swift_user_name.find(':');
  if (std::string::npos == pos) {
    return swift_user_name;
  } else {
    return swift_user_name.substr(pos + 1);
  }
}




bool
SignedMachineEngine::is_applicable(const req_state* const state) const
{
  return (state->info.env->get("HTTP_X_AUTH_USER", nullptr) != nullptr);
}


bool
SignedMachineEngine::valid_shared_key(const req_state* const state) const
{

  const char* secret = state->info.env->get("HTTP_X_AUTH_KEY", nullptr);
  
  if(   secret == nullptr 
     || std::strlen(secret) != SignedMachineEngine::AUTH_KEY_LENGTH)
  {
    return false; // wrong shared key
  }

  // check if the shared keys match
  return true; // for the first implementation always matches
  

}


SignedMachineEngine::result_t
SignedMachineEngine::authenticate(const req_state* const state) const
{

  if(!valid_shared_key(state))
   {
     return result_t::deny(-EPERM);
   }


  // check if there is a user name
  if(!is_applicable(state))
  {
    return result_t::deny(-EPERM);
  }


  // process the request
  RGWUserInfo user_info;
  
  // there must be a user since we have passed 
  // the 'is_applicable()' method.
  std::string user_str(state->info.env->get("HTTP_X_AUTH_USER", ""));

  int ret = rgw_get_user_info_by_swift(store_, user_str, user_info);
  if (ret < 0)
  {
    throw ret;
  }
  
  // found a valid user and its info 
  // has been retrieved
  // check if it has any secret keys
  const auto siter = user_info.swift_keys.find(user_str);
  
  // user has no Swift keys --> cannot perform IO
  if(siter == user_info.swift_keys.end())
  {
    return result_t::deny(-EPERM);
  }

  auto apl = apl_factory_->create_apl_local(cct_, state, user_info,
                 extract_swift_subuser_sing(user_str));


  return result_t::grant(std::move(apl));
  

}


} /* namesapce singsotrage */
} /* namespace auth */
} /* namespace rgw */

void 
RGW_SINGSTORAGE_Auth_Get::execute()
{

  int ret = -EPERM;
  uint64_t err_code = rgw::singstorage::SINGErrorCode::USER_ERR; // no such username  

  const char* key    = s->info.env->get("HTTP_X_AUTH_KEY", nullptr);
  const char* user   = s->info.env->get("HTTP_X_AUTH_USER", nullptr);
  const char* tranID = s->info.env->get("HTTP_X_TRAN_ID", nullptr);


  //assert(key && user && tranID);
  assert(tranID);

  s->prot_flags |= RGW_REST_SWIFT; // use SWIFT flags as it matches
                                   // our settings and no need to
                                   // modify external classes
  

  string user_str;
  RGWUserInfo info;
  bufferlist bl; 
  RGWAccessKey* sing_key;
  map<string, RGWAccessKey>::iterator siter;
  string tenant_path; // tenant path
  


  if(!key || !user) 
  {
    ret = -EACCES;
    goto done_get_sing_auth; // no authentication possible
  }
    

  user_str = user; // copy to string for later comparison


  // use SWIFT authentication procedure as it matches our current
  // desing. Later, we need to change and move to our own system.
  if((ret = rgw_get_user_info_by_swift(store, user_str, info)) < 0)
  {
    // user error (no such user) 
    //err_code = rgw::singstorage::SINGErrorCode::INTERNAL_ERR; 
    ret = -EACCES;
    goto done_get_sing_auth;
  }


  siter = info.swift_keys.find(user_str);
  if(siter == info.swift_keys.end()) // no such user found
  {
    goto done_get_sing_auth;
  }


  sing_key = &siter->second;
 
  if(sing_key->key.compare(key) != 0)
  {
    //dout (0) << "NOTICE: RGW_SING_Auth_Get::execute(): bad singstorage key" << dendl;
    err_code = rgw::singstorage::SINGErrorCode::PASSWD_ERR;
    goto done_get_sing_auth;
  }

  // success
  ret = STATUS_ACCEPTED;

  // send the tenant path
 
  if(info.user_id.tenant.empty())
  {
    tenant_path = info.user_id.id;
  }
  else
  {
    tenant_path = std::move(info.user_id.tenant + ":" + info.user_id.id);
  }
  

done_get_sing_auth:

  // add the HTTP headers
  dump_header(s, "X-Tran-Id", tranID); 
  set_req_state_err(s, ret);
  dump_errno(s);


  
  // need to encode the the error type or tenant_path
  assert(s->formatter && s->format == RGW_FORMAT_JSON);
  
  s->formatter->open_object_section(""); // empty JSON wrapper
 
  s->formatter->open_object_section("Result");
  
  if(ret == STATUS_ACCEPTED) // account
  {
    s->formatter->dump_string("Account", tenant_path);
  }
  else // some error occured
  {
    s->formatter->dump_unsigned("Error_Type", err_code);
  }

  s->formatter->close_section(); // close JSON object 'Result'
 
  s->formatter->close_section(); // close the wrapper
  

  // complete the header and dump the body
  end_header(s, nullptr, nullptr, NO_CONTENT_LENGTH, true, false);
 
}


int
RGWHandler_SINGSTORAGE_Auth::init(RGWRados* store, 
                                  struct req_state* state,
                                  rgw::io::BasicClient* cio)
{
  state->dialect    =  "singstore-auth";
  state->formatter  =  new JSONFormatter(false);
  state->format     =  RGW_FORMAT_JSON;
  //state->op         =  OP_GET; /* Not sure about this */

  return RGWHandler::init(store, state, cio);

}

RGWOp* 
RGWHandler_SINGSTORAGE_Auth::op_get()
{
  return new RGW_SINGSTORAGE_Auth_Get;
}

