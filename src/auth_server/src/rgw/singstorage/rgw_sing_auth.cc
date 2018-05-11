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



// OpenSSL
#include <openssl/sha.h>
static constexpr const size_t SING_AUTH_HASH_KEY_LEN = static_cast<const size_t>(SHA256_DIGEST_LENGTH);



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
     || std::strlen(secret) != SING_AUTH_HASH_KEY_LEN)
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




bool
RGW_SINGSTORAGE_Auth_Get::search_key(const std::map<std::string, RGWAccessKey>& auth_keys, const char* hash_val) const noexcept
{

  // make sure the key of the proper length
  if(hash_val == nullptr || std::strlen(hash_val) != SING_AUTH_HASH_KEY_LEN)
  {
    return false; // the length is not correct
  }


  // compute sha256 hash value 
  unsigned char key_secret[SHA256_DIGEST_LENGTH + 1];
  SHA256_CTX sha_tmp_ctx;
  int hash_res = 0;  

  // loop over all keys and compute their hash
  for(const auto& map_item : auth_keys)
  {
    hash_res = SHA256_Init(&sha_tmp_ctx);
    
    if(!hash_res)
    {
      return false; // some internal error
    }   

    hash_res = SHA256_Update(&sha_tmp_ctx, 
      reinterpret_cast<const unsigned char*>(map_item.first.c_str()),
      map_item.first.length());

    if(!hash_res)
    {
      return false; // some internal error
    }

    hash_res = SHA256_Final(key_secret, &sha_tmp_ctx);

    if(!hash_res)
    {
      return false; // some internal error
    }

    // do simple string comparison
    key_secret[SHA256_DIGEST_LENGTH] = '\0'; // make a string
    
    if(std::strcmp(hash_val, reinterpret_cast<const char*>(key_secret)) == 0)
    {
      return true;
    }
   
  }// for


  return false; // didn't find a match
}



bool
RGW_SINGSTORAGE_Auth_Get::check_key(const char* swift_key, const char* hash_val) const noexcept
{

  // need to check if the hash of swift_key matches
  // hash_val
  if(swift_key == nullptr || hash_val == nullptr || 
     std::strlen(hash_val) != SING_AUTH_HASH_KEY_LEN)
  {
    return false; // not a match
  }

  // compute the hash of the swift_key
  unsigned char swift_hash_val[SHA256_DIGEST_LENGTH + 1];

  SHA256_CTX sha_ctx_tmp;
  int hash_res = SHA256_Init(&sha_ctx_tmp);

  if(!hash_res)
  {
    return false; // need to log this
  }

  hash_res = SHA256_Update(&sha_ctx_tmp,
             reinterpret_cast<const unsigned char*>(swift_key),
             std::strlen(swift_key));

  if(!hash_res)
  {
    return false; // need to log this case
  }
  

  // finally, compute digest
  hash_res = SHA256_Final(swift_hash_val, &sha_ctx_tmp);

  if(!hash_res)
  {
    return false; // need to log this case
  }

  // do string comparison
  swift_hash_val[SHA256_DIGEST_LENGTH] = '\0'; // make a C string
  
  return (std::strcmp(hash_val, 
          reinterpret_cast<const char*>(swift_hash_val)) == 0) ; 


}




void 
RGW_SINGSTORAGE_Auth_Get::execute()
{

  int ret = -EPERM;
  uint64_t err_code = rgw::singstorage::SINGErrorCode::USER_ERR; // no such username  

  const char* key    = s->info.env->get("HTTP_X_AUTH_KEY",  nullptr);
  const char* user   = s->info.env->get("HTTP_X_AUTH_USER", nullptr);
  const char* tranID = s->info.env->get("HTTP_X_TRAN_ID",   nullptr);


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
  // design. Later, we need to change and move to our own system.
  if((ret = rgw_get_user_info_by_swift(store, user_str, info)) < 0)
  {
    // user error (no such user) 
    //err_code = rgw::singstorage::SINGErrorCode::INTERNAL_ERR; 
    ret = -EACCES;
    goto done_get_sing_auth;
  }



  /*siter = info.swift_keys.find(user_str);
  if(siter == info.swift_keys.end()) // no such user found
  {
    goto done_get_sing_auth;
  }

  // the sent auth value is a hashed password
  // need to go over all strings an hash them
 

  sing_key = &siter->second;
  */

  // This version supports one subuser per user
  // so need to iterate over all swift_keys 
  // and check if any of them match.

  bool found_match = false; // if any of the keys matched

  for(const auto& auth_check : info.swift_keys)
  {
    if((found_match = check_key(auth_check.second.key.c_str(), key)) == true)
    {
      break; // found a match
    }
  
  }

  if(!found_match)
  {
    //checked all keys, none of them mathched
    err_code = rgw::singstorage::SINGErrorCode::PASSWD_ERR;
    goto done_get_sing_auth;

  }

  // the sent auth value is a hashed password
  // need to check if the value matches the retrieved
  // key (compute hash and compare) 
  /*
  if(!check_key(sing_key->key.c_str(), key))
  {
    //dout (0) << "NOTICE: RGW_SING_Auth_Get::execute(): bad singstorage key" << dendl;
    err_code = rgw::singstorage::SINGErrorCode::PASSWD_ERR;
    goto done_get_sing_auth;
  }*/

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

