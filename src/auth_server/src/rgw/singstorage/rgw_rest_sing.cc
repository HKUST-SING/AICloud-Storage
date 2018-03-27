// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include <boost/algorithm/string/predicate.hpp>
#include <boost/format.hpp>
#include <boost/optional.hpp>
#include <boost/utility/in_place_factory.hpp>

#include "include/assert.h"
#include "ceph_ver.h"

#include "common/Formatter.h"
#include "common/utf8.h"
#include "common/ceph_json.h"

#include "rgw_rest_sing.h"
#include "rgw_acl_swift.h"
#include "rgw_formats.h"
#include "rgw_client_io.h"

#include "rgw_auth.h"
#include "rgw_sing_auth.h"

#include "rgw_request.h"
#include "rgw_process.h"

#include <array>
#include <sstream>
#include <memory>

#include <boost/utility/string_ref.hpp>

int RGWHandler_REST_SING::authorize()
{
  return rgw::auth::Strategy::apply(auth_strategy, s);
}

int RGWHandler_REST_SING::postauth_init()
{
  struct req_init_state* t = &s->init_state;

  /* XXX Stub this until Sing Auth sets account into URL. */
  s->bucket_tenant = s->user->user_id.tenant;
  s->bucket_name = t->url_bucket;

  dout(10) << "s->object=" <<
    (!s->object.empty() ? s->object : rgw_obj_key("<NULL>"))
           << " s->bucket="
     << rgw_make_bucket_entry_name(s->bucket_tenant, s->bucket_name)
     << dendl;

  int ret;
  ret = rgw_validate_tenant_name(s->bucket_tenant);
  if (ret)
    return ret;
  ret = validate_bucket_name(s->bucket_name);
  if (ret)
    return ret;
  ret = validate_object_name(s->object.name);
  if (ret)
    return ret;

  if (!t->src_bucket.empty()) {
    /*
     * We don't allow cross-tenant copy at present. It requires account
     * names in the URL for Sing.
     */
    s->src_tenant_name = s->user->user_id.tenant;
    s->src_bucket_name = t->src_bucket;

    ret = validate_bucket_name(s->src_bucket_name);
    if (ret < 0) {
      return ret;
    }
    ret = validate_object_name(s->src_object.name);
    if (ret < 0) {
      return ret;
    }
  }

  return 0;
}

int RGWHandler_REST_SING::validate_bucket_name(const string& bucket)
{
  const size_t len = bucket.size();

  if (len > MAX_BUCKET_NAME_LEN) {
    /* Bucket Name too long. Generate custom error message and bind it
     * to an R-value reference. */
    const auto msg = boost::str(
      boost::format("Container name length of %lld longer than %lld")
        % len % int(MAX_BUCKET_NAME_LEN));
    set_req_state_err(s, ERR_INVALID_BUCKET_NAME, msg);
    return -ERR_INVALID_BUCKET_NAME;
  }

  const auto ret = RGWHandler_REST::validate_bucket_name(bucket);
  if (ret < 0) {
    return ret;
  }

  if (len == 0)
    return 0;

  if (bucket[0] == '.')
    return -ERR_INVALID_BUCKET_NAME;

  if (check_utf8(bucket.c_str(), len))
    return -ERR_INVALID_UTF8;

  const char *s = bucket.c_str();

  for (size_t i = 0; i < len; ++i, ++s) {
    if (*(unsigned char *)s == 0xff)
      return -ERR_INVALID_BUCKET_NAME;
  }

  return 0;
}

static void next_tok(string& str, string& tok, char delim)
{
  if (str.size() == 0) {
    tok = "";
    return;
  }
  tok = str;
  int pos = str.find(delim);
  if (pos > 0) {
    tok = str.substr(0, pos);
    str = str.substr(pos + 1);
  } else {
    str = "";
  }
}

int RGWHandler_REST_SING::init_from_header(struct req_state* const s,
                                            const std::string& frontend_prefix)
{
  string req;
  string first;

  s->prot_flags |= RGW_REST_SWIFT;

  char reqbuf[frontend_prefix.length() + s->decoded_uri.length() + 1];
  sprintf(reqbuf, "%s%s", frontend_prefix.c_str(), s->decoded_uri.c_str());
  const char *req_name = reqbuf;



  // This may not applicable in our case.
  /*
  const char *p;

  if (*req_name == '?') {
    p = req_name;
  } else {
    p = s->info.request_params.c_str();
  }

  s->info.args.set(p);
  s->info.args.parse(); */

 

  /* Skip the leading slash of URL hierarchy. */
  if (req_name[0] != '/')
  {

    s->format = RGW_FORMAT_JSON;
    s->formatter = new JSONFormatter;
    return -ERR_BAD_URL;
  } 
  else 
  {
    req_name++;
  }

  if ('\0' == req_name[0]) {
    s->format = RGW_FORMAT_JSON;
    s->formatter = new JSONFormatter;
    return -ERR_BAD_URL;

  }

  req = req_name;

  auto pos = req.find('/');

  if (std::string::npos != pos) {
    first = req.substr(0, pos); // protocol prefix

    if (first.compare(RGWHandler_REST_SING::STORAGE_PREFIX) == 0) 
    {
      next_tok(req, first, '/');
    }
    else
    {
      s->format = RGW_FORMAT_JSON;
      s->formatter = new JSONFormatter;
      return -ERR_BAD_URL;
    }
  } 
  else
  {
    s->format = RGW_FORMAT_JSON;
    s->formatter = new JSONFormatter;
    return -ERR_BAD_URL;
  }
  

  std::string tenant_path;

  int ret = allocate_formatter(s, RGW_FORMAT_PLAIN, true);
  if (ret < 0)
    return ret;


  std::string account_name;
  next_tok(req, account_name, '/');

  if (account_name.empty()) 
  {
      return -ERR_PRECONDITION_FAILED;
  } 
  else 
  {
     s->account_name = account_name;
  }
  

  next_tok(req, first, '/');

  dout(10) << "ver=" << ver << " first=" << first << " req=" << req << dendl;
  if (first.size() == 0 || req.size() == 0)
  {
    s->format = RGW_FORMAT_JSON;
    s->formatter = new JSONFormatter;
    return -ERR_BAD_URL;
  }

  s->info.effective_uri = "/" + first;

  // Save bucket to tide us over until token is parsed.
  s->init_state.url_bucket = first;

  s->object =
      rgw_obj_key(req, s->info.env->get("HTTP_X_OBJECT_VERSION_ID", "")); /* rgw sing extension */
  s->info.effective_uri.append("/" + s->object.name);
  

  return 0;
}

int RGWHandler_REST_SING::init(RGWRados* store, struct req_state* s,
        rgw::io::BasicClient *cio)
{
  struct req_init_state *t = &s->init_state;

  s->dialect = "sing";

  std::string copy_source =
    url_decode(s->info.env->get("HTTP_X_COPY_FROM", ""));
  if (! copy_source.empty()) {
    bool result = RGWCopyObj::parse_copy_location(copy_source, t->src_bucket,
              s->src_object);
    if (!result)
      return -ERR_BAD_URL;
  }

  if (s->op == OP_COPY) {
    std::string req_dest =
      url_decode(s->info.env->get("HTTP_DESTINATION", ""));
    if (req_dest.empty())
      return -ERR_BAD_URL;

    std::string dest_bucket_name;
    rgw_obj_key dest_obj_key;
    bool result =
      RGWCopyObj::parse_copy_location(req_dest, dest_bucket_name,
              dest_obj_key);
    if (!result)
       return -ERR_BAD_URL;

    std::string dest_object = dest_obj_key.name;

    /* convert COPY operation into PUT */
    t->src_bucket = t->url_bucket;
    s->src_object = s->object;
    t->url_bucket = dest_bucket_name;
    s->object = rgw_obj_key(dest_object);
    s->op = OP_PUT;
  }

  return RGWHandler_REST::init(store, s, cio);
}

RGWHandler_REST*
RGWRESTMgr_SING::get_handler(struct req_state* const s,
                              const rgw::auth::StrategyRegistry& auth_registry,
                              const std::string& frontend_prefix)
{
  int ret = RGWHandler_REST_SING::init_from_header(s, frontend_prefix);
  if (ret < 0) {
    ldout(s->cct, 10) << "init_from_header returned err=" << ret <<  dendl;
    return nullptr;
  }

  const auto& auth_strategy = auth_registry.get_sing();

  if (s->init_state.url_bucket.empty() || s->object.empty()) 
  {
    return nullptr; /*The state must refer to an object*/
  }


  return new RGWHandler_REST_Obj_SING(auth_strategy);
}


/********************* RGWOps ****************************/
int
RGWDeleteObj_ObjStore_SING::verify_permission()
{
  op_ret = RGWDeleteObj_ObjStore::verify_permission();

  /**
   * User has no access if s/he is not authorized
   * to perform DELETE/WRITE operations.
   */
  if(op_ret == -EACCES || op_ret == -EPERM) 
                       // no authorization
  {
    return -EPERM;
  }
  else
  {
   return op_ret; // may contain some other error code
  }

}


int
RGWDeleteObj_ObjStore_SING::get_params()
{
  const string& mm = s->info.args.get("multipart-manifest");

  multipart_delete = (mm.compare("delete") == 0);

  return RGWDeleteObj_ObjStore::get_params();

}


void 
RGWDeleteObj_ObjStore_SING::send_response()
{
  int r = op_ret;

  if(multipart_delete)
  {
    r= 0;
  }
  else if(!r)
  {
    r = STATUS_NO_CONTENT;
  }

  // get transaction ID
  const char* tranID = s->info.env->get("HTTP_X_TRAN_ID");
  
  assert(tranID); // make sure there is a transaction ID

  set_req_state_err(s, r);
  dump_errno(s);
  dump_header(s, "HTTP_X_TRAN_ID", tranID); 

  if(multipart_delete)
  {
    end_header(s, this /* RGWop */, nullptr, /* contype */, CHUNKED_TRANSFER_ENCODING);

    if(deleter)
    {
      bulkdelete_respond(deleter->get_num_deleted(),
                         deleter->get_num_unfound(),
                         deleter->get_failures(),
                         s->prot_flags,
                         *s->formatter);
    } // if
    else
    {
      if(-ENOENT == op_ret)
      {
        bulkdelete_respond(0,1,{}, s->prot_flags,
                           *s->formatter);
      } //if  
      else
      {
        RGWBulkDelete::acct_path_t path;
        path.bucket_name = s->bucket_name;
        path.obj_key     = s->object; 

        RGWBulkDelete::fail_desc_t fail_desc;
        fail_desc.err  = op_ret;
        fail_desc.path = path;

        bulkdelete_respond(0, 0, {fail_desc}, 
                           s->prot_flags, *s->formatter);        
      }//else
    }// else
  }// if
  else
  {
    end_header(s, this);
  } // else
 


  rgw_flush_formatter_and_reset(s, s->formatter);
}
/********************* RGWOps End ***********************/



RGWHandler_REST* RGWRESTMgr_SING_Info::get_handler(
  struct req_state* const s,
  const rgw::auth::StrategyRegistry& auth_registry,
  const std::string& frontend_prefix)
{
  s->prot_flags |= RGW_REST_SWIFT;
  const auto& auth_strategy = auth_registry.get_sing();
  return new RGWHandler_REST_SING_Info(auth_strategy);
}
