#ifndef CEPH_RGW_SINGSTORAGE_AUTH_H
#define CEPH_RGW_SINGSTORAGE_AUTH_H

/**
 * Class uses a very similar way of authenticating storage users
 * to the one used by the provided SWIFT implementation. In fact,
 * the system uses a lot of the swift code to authenticate users.
 */


#include "../rgw_client_io.h"
#include "../rgw_http_client.h"
#include "../rgw_rest.h"
#include "rgw_sing_auth.h"



#define DEFAULT_SINGSTORAGE_PREFIX "/sing"
#define AUTH_USER 0x01 // no such user found
#define AUTH_PASS 0x02 // password is incorrect
#define AUTH_ERR  0x10 // some internal error

// HTTP Status Codes
#define HTTP_OK 200
#define HTTP_UNAUTHORIZED 401
#define HTTP_NOT_FOUND 404

namespace rgw {
namespace auth {
namespace singstorage { 


bool
SignedMachineEngine::is_applicable(const std::string& token) const noexcept
{

  if(token.empty())
  {
    return false;
  }
  else
  {
    return (token.compare(0, 10, "AUTH_rgwk") == 0);
  }


}



bool
SignedMachineEngine::valid_shared_key(const std::string& secret) const
{

  if(secret.length() != SignedMachineEngine::AUTH_KEY_LENGTH)
  {
    return false; // wrong shared key
  }

  // check if the shared keys match
  return true; // for the first implementation always matches
  

}

std::pair<std::string, std::string>
SignedMachineEngine::extract_auth_data(const std::string& token) const
{

  // Logic is very simple: the fields until the first colon is
  // the username, everything after the semicolon is the secret 
  // shared key


  std::string usr_key = token.substr(strlen("AUTH_rgwtk"));
  
  auto con_pos = usr_key.find(':'); // find the first occurrence of ':'

  // check if the it is not the end
  if(con_pos == std::string::npos ||
     con_pos == std::string::size_type(0) ||
     ++con_pos == std::string::npos)
  {
    // return an empty pair
    return std::move(std::make_pair<std::string, std::string>("", ""));
  }


  // retrieve the username and the secret shared key from
  // the token field
  std::string username(std::move(token.substr(0, con_pos)));
  std::string key(std::move(token.substr(con_pos)));

  // return the parsed values
  return std::move(std::make_pair<std::string, std::string>(std::move(username), std::move(key)));


}


SignedMachineEngine::result_t
SignedMachineEngine::authenticate(const std::string& token,
                                  const req_state* const state) const

{

  if(!is_applicable(token))
   {
     return result_t::deny(-EPERM);
   }

  // extract information from the token
  // (username and secret shared key)

  std::pair<std::string, std::string> vals = std::move(extract_data(token));

  // check if the parsed values are valid
  if(!vals.first.length() || !vals.second.length())
  {
    return result_t::deny(-EPERM); // something wring internally
  }


  // check if the shared secret key is valid
  if(!valid_shared_key(vals.second))
  {
    // wrong secret key
    return result_t::deny(-EPERM); 
  }


  // process the request
  RGWUserInfo user_info;


  int ret = rgw_get_user_info_by_swift(store_, vals.first, user_info);
  if (ret < 0)
  {
    throw ret;
  }
  

  // found a valid user
  const auto siter = user_info.swift_keys.find(vals.first);

  if(siter == std::end(user_info.swift_keys))
  {
    return result_t::deny(-EPERM);
  }


  auto apl = apl_factory->create_apl_local(cct_, state, user_info,
                                           extract_swift_subuser(vals.first));


  return result_t::grant(std::move(apl));
  

}


} /* namesapce singsotrage */
} /* namespace auth */
} /* namespace rgw */

void 
RGW_SINGSTORAGE_Auth_Get::execute()
{

  int ret = -EPERM;
  int err_code = AUTH_ERR;  

  const char* key    = s->info.env->get("HTTP_X_AUTH_KEY");
  const char* user   = s->info.env->get("HTTP_X_AUTH_USER");
  const char* tranID = s->info.env->get("HTTP_X_TRAN_ID");

  s->prot_flags |= RGW_REST_SWIFT; // use SWIFT flags as it matches
                                   // our settings and no need to
                                   // modify external classes
  

  string user_str;
  RGWUserInfo info;
  bufferlist bl; 
  RGWAcessKey* sing_key;
  map<string, RGWAccessKey>::iterator siter;




  if(!key || !user) 
  {
    err_code = AUTH_USER; // no user provided
    goto done; // no authentication possible
  }
    

  user_str = user; // copy to string for later comparison


  // use SWIFT authentication procedure as it matches our current
  // desing. Later, we need to change and move to our own system.
  if((ret = rgw_get_user_info_by_swift(store, user_str, info)) < 0)
  {
    // internal error 
    goto done;
  }


  siter = info.swift_keys.find(user_str);
  if(siter == info.swift_keys.end()) // no such user found
  {
    err_code = AUTH_USER;
    goto done;
  }


  sing_key = &siter->second;
 
  if(sing_key->key.compare(key) != 0)
  {
    //dout (0) << "NOTICE: RGW_SING_Auth_Get::execute(): bad singstorage key" << dendl;
    err_code = AUTH_PASS;
    goto done;
  }

  // success
  ret = STATUS_ACCEPTED;

  // send the tenant path
  std::string tenant_path(std::move(info.user_id.to_str()));
  


done:

  // add the HTTP headers
  dump_header(s, "HTTP_X_TRAN_ID", tranID); 
  set_req_state_err(s, ret);
  dump_errno(s);


  
  // need to encode the the error type or tenant_path
  assert(s->formatter & s->format == RGW_FORMAT_JSON);
    
  s->formatter->open_object_section("Result");
  
  if(ret == STATUS_ACCEPTED) // account
  {
    s->formatter->dump_string("Account", tenant_path);
  }
  else // some error occured
  {
    s->formatter->dump_int("Error_Type", err_code);
  }

  s->formatter->close_section(); // close JSON object
  

  // complete the header and dump the body
  end_header(s, nullptr, nullptr, NO_CONTENT_LENGTH, true, false);
 
}


int
RGWHandler_SINGSTORAGE_Auth::init(RGWRados* store, 
                                  struct req_state* state,
                                  rgw::io::BasicClient* cio)
{
  state->dialect    =  "singstore-auth";
  state->formatter  =  new JSONFormatter;
  state->format     =  RGW_FORMAT_JSON;
  state->op         =  OP_GET; /* Not sure about this */

  return RGWHandler::init(store, state, cio);

}

RGWOp* 
RGWHandler_SINGSTORAGE_Auth::op_get()
{
  return new RGW_SINGSTORAGE_Auth_Get;
}

