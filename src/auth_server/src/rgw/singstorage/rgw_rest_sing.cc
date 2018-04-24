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
#include "../rgw_acl_swift.h"
#include "../rgw_formats.h"
#include "../rgw_client_io.h"

#include "../rgw_auth.h"
#include "rgw_sing_auth.h"
#include "rgw_sing_error_code.h"

#include "../rgw_request.h"
#include "../rgw_process.h"

#include <array>
#include <sstream>
#include <memory>
#include <limits>
#include <cstdlib>
#include <algorithm>

#include <boost/utility/string_ref.hpp>


#define dout_context g_ceph_context
#define dout_subsys  ceph_subsys_rgw

#define SING_HTTP_OK 200

// get maximum interger value
static constexpr const size_t MAX_INT_VAL = static_cast<const size_t>(std::numeric_limits<int>::max());

static const string SING_SHADOW_NS = RGW_OBJ_NS_SHADOW;

const std::string RGWHandler_REST_SING::STORAGE_PREFIX = "sing";
const std::string RGWPost_Manifest_SING::EMPTY_STRING  =   "";

// default data bucket name
// for testing is enough
#define DEFAULT_DATA_POOL "default.rgw.buckets.data"



static void bulkdelete_respond(const unsigned num_deleted,
                               const unsigned int num_unfound,
                               const std::list<RGWBulkDelete::fail_desc_t>& failures,
                               const int prot_flags,                  /* in  */
                               ceph::Formatter& formatter)            /* out */
{
  formatter.open_object_section("");       // empty object
  formatter.open_object_section("Result"); // 'Result' object

  string resp_status;
  string resp_body;

  if (!failures.empty()) {
    int reason = ERR_INVALID_REQUEST;
    for (const auto fail_desc : failures) {
      if (-ENOENT != fail_desc.err && -EACCES != fail_desc.err) {
        reason = fail_desc.err;
      }
    }
    rgw_err err;
    set_req_state_err(err, reason, prot_flags);
    dump_errno(err, resp_status);
  } else if (0 == num_deleted && 0 == num_unfound) {
    /* 400 Bad Request */
    dump_errno(400, resp_status);
    resp_body = "Invalid bulk delete.";
  } else {
    /* 200 OK */
    dump_errno(200, resp_status);
  }

  encode_json("Number_Deleted", num_deleted, &formatter);
  encode_json("Number_Not_Found", num_unfound, &formatter);
  encode_json("Response_Body", resp_body, &formatter);
  encode_json("Response_Status", resp_status, &formatter);

  formatter.open_array_section("Errors");
  for (const auto fail_desc : failures) {
    formatter.open_array_section("object");

    stringstream ss_name;
    ss_name << fail_desc.path;
    encode_json("Name", ss_name.str(), &formatter);

    rgw_err err;
    set_req_state_err(err, fail_desc.err, prot_flags);
    string status;
    dump_errno(err, status);
    encode_json("Status", status, &formatter);
    formatter.close_section();
  }

  formatter.close_section(); // close 'Errors'
  formatter.close_section(); // close 'Result'
  formatter.close_section(); // close '' (empty object)
}




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

  int ret = allocate_formatter(s, RGW_FORMAT_JSON, false);
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

  dout(10) << "first=" << first << " req=" << req << dendl;

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
      rgw_obj_key(req); /* rgw sing extension */
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
  const char* tranID = s->info.env->get("HTTP_X_TRAN_ID", nullptr);
  
  assert(tranID); // make sure there is a transaction ID

  set_req_state_err(s, r);
  dump_errno(s);
  dump_header(s, "X-Tran-Id", tranID); 

  if(multipart_delete)
  {
    end_header(s, this /* RGWop */, nullptr /* contype */, CHUNKED_TRANSFER_ENCODING);

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



uint64_t
RGWGetObjLayout_SING::get_sing_error(const int sys_error)
{

  const int check_err = std::abs(sys_error);

  switch(check_err)
  {
    case EPERM:
    {
      return rgw::singstorage::SINGErrorCode::ACL_ERR;
    }
  
    case EACCES:
    case ERR_NO_SUCH_BUCKET:
    case ENOENT:
    {
      return  rgw::singstorage::SINGErrorCode::PATH_NOT_FOUND;

    }

   default:
     return rgw::singstorage::SINGErrorCode::INTERNAL_ERR; 

  } // switch

}



int
RGWGetObjLayout_SING::verify_permission()
{
  // need to verify if the user can
  // read the data
 if(!verify_object_permission(s, rgw::IAM::s3GetObject))
 {
   // cannot access data (no permission granted)
   return -EPERM;
 }
  
  return 0;
}


void
RGWGetObjLayout_SING::execute()
{

  // execute the parent operation first
  RGWGetObjLayout::execute();
  
  // check the status
  if(op_ret < 0)
  {
    rawObjs_.clear();
    return; // stop processing further
  }

  // retrieved manifest
  // get all raw objects for reading
  rgw_raw_obj read_obj;


  if(manifest) // has manifest
  {
    auto iter    = manifest->obj_begin();
    auto man_end = manifest->obj_end();   


    for(; iter != man_end; ++iter)
    {
      read_obj = iter.get_location().get_raw_obj(store);
     
      if(read_obj.empty())
      {
        op_ret = -ERR_INTERNAL_ERROR;
        // clear all objects
        rawObjs_.clear();
   
        return;
      }

      // push the value to the vector
      rawObjs_.push_back(std::move(RGWGetObjLayout_SING::ReadObjInfo(read_obj, iter.get_stripe_size())));
     
    } //for
  }//if
  else // no manifest (impossible)
  {
    op_ret = -ERR_INTERNAL_ERROR;
    rawObjs_.clear();
  }


}


void
RGWGetObjLayout_SING::send_response()
{


  const char* tranID = s->info.env->get("HTTP_X_TRAN_ID", nullptr);
  assert(tranID);
  
  dump_header(s, "X-Tran-Id", tranID);
  
  if(manifest && op_ret >=0)
  {
    set_req_state_err(s, SING_HTTP_OK);
    dump_errno(s);
  }
  

  else{
    set_req_state_err(s, op_ret);
    dump_errno(s);
  }


  s->formatter->open_object_section(""); // empty object for
                                         // wrapping

  s->formatter->open_object_section("Result");

   if(!manifest || op_ret < 0)
  {
    ldout(s->cct, 20) << "NOTICE: send_response() " 
    << "manifest is 'nullptr'" << dendl;  

    const uint64_t sing_err = get_sing_error(op_ret);
    s->formatter->dump_unsigned("Error_Type", sing_err);
  }
  else
  {

    // first of all, add total object size
    s->formatter->dump_unsigned("Object_Size", manifest->get_obj_size());

    // send a JSON Array of Rados Objects
    s->formatter->open_array_section("Rados_Objs");
    
    for(const auto& rad_obj : rawObjs_)
    {
      s->formatter->open_object_section("Object");
      // store object info
      rad_obj.obj_info.dump(s->formatter);
      s->formatter->dump_unsigned("size", rad_obj.obj_size);
      // Rados object has completed
      s->formatter->close_section();

    }  

    // close 'Rados_Objs' array
    s->formatter->close_section();

  }

  s->formatter->close_section(); // close 'Result' field
  s->formatter->close_section(); // close the wrapper


  // clear raw object
  rawObjs_.clear(); 

  // send the body
  end_header(s, nullptr, nullptr, NO_CONTENT_LENGTH, true, false);

}


void
RGWPutObj_ObjStore_SING::init(RGWRados* store, struct req_state* state,
                              RGWHandler* dialect_handler)
{

  // create your own handlers
  get_op_ = new RGWGetObjLayout_SING;

  
  assert(get_op_);


  // initialize the operations for future
  // use
  get_op_->init(store, state, dialect_handler);

  

  RGWPutObj_ObjStore::init(store, state, dialect_handler); 

}



int 
RGWPutObj_ObjStore_SING::create_bucket()
{

  // create an object of the SING_CreateBucket class
  // so that it would handle the creation procedure
  auto op_ptr = new RGWPutObj_ObjStore_SING::SING_CreateBucket;
  assert(op_ptr);

  // initialize the object
  op_ptr->init(store, s, dialect_handler); 


  s->op_type = op_ptr->get_type();

  int res_code = op_ptr->init_processing();
 
  if(res_code < 0)
  {
    delete op_ptr;
    op_ptr = nullptr;

    return res_code;
  }

  res_code = op_ptr->verify_permission();
 
  if(res_code < 0)
  {
    delete op_ptr;
    op_ptr = nullptr;
    
    return res_code;
  }

  res_code = op_ptr->verify_params();
 
  if(res_code < 0)
  {
   delete op_ptr;
   op_ptr = nullptr;

   return res_code;
  }

  op_ptr->pre_exec();
  op_ptr->execute();
  op_ptr->complete();


  res_code = op_ptr->get_ret();


  // clean up the operation
  delete op_ptr;
  op_ptr = nullptr;

  // reset the opearation type  
  s->op_type = RGWPutObj_ObjStore::get_type();


  // return the result of creation
  return res_code;

}



int
RGWPutObj_ObjStore_SING::verify_permission()
{
  if(!bucket_exists)
  {
    op_ret = -404; // not found

    sing_err = sing_err_name::PATH_NOT_FOUND;

    return -404;
  }

  int res = RGWPutObj_ObjStore::verify_permission();
  if (res < 0)
  {
    op_ret = -EPERM;
    
    sing_err = sing_err_name::ACL_ERR;

    return -EPERM;
  }


  return 0; // success
}


void
RGWPutObj_ObjStore_SING::pre_exec() 
{
  RGWPutObj_ObjStore::pre_exec();
}



uint64_t
RGWPutObj_ObjStore_SING::get_obj_size()
{

  uint64_t res_len = 0;
  sing_err = sing_err_name::INTERNAL_ERR;

  if(s->content_length <=0)
  { 
    sing_err = sing_err_name::SMALL_ERR;
    return 0; // no content
  }

  
  const size_t need_read = static_cast<size_t>(s->content_length);

  if(need_read > MAX_INT_VAL)
  {
    sing_err = sing_err_name::LARGE_ERR;
    return 0; // too big content
  }  

  size_t left_read = need_read;

  char* json_buffer = new char[need_read]; // need to read

  size_t read_bytes = 0;
  int len = 0;

  while(left_read)
  {
    len = recv_body(s, (json_buffer+read_bytes), left_read);

    if(len < 0)
    {
      // avoid using goto
      delete[] json_buffer; 
      return 0;
    }
   
    // cast int to size_t
    const size_t got_data = static_cast<const size_t>(len);


    // update read for pointers
    read_bytes += got_data;
    if(got_data > left_read)
    {
      break;
    }

    left_read -= got_data;

  }// while
  

  // decode the char array into a JSON object.
  JSONParser parser;
  
  bool ret = parser.parse(json_buffer, static_cast<int>(need_read));
 
  if(!ret)
  {
    delete[] json_buffer;
    return 0;  
  }


  auto iter = parser.find_first("Result"); // find a JSON
                                           // object that 
                                           // encapsultes 
                                           // object size
  if(iter.end())
  { // did not find
    delete[] json_buffer;
    return 0;
  }

  auto obj_size = (*iter)->find_first("Object_Size"); // look for 
                                                   // object size


  unsigned long long size_val = 0ULL; // for reading size

  if(obj_size.end())
  {
    delete[] json_buffer;
    return 0;
  }


  ::decode_json_obj(size_val, *obj_size);

  // cast to uint64_t
  res_len = static_cast<uint64_t>(size_val);

  
  // no errors
  sing_err = sing_err_name::SUCCESS;
  delete[] json_buffer;
  
  return res_len;  

}

void 
RGWPutObj_ObjStore_SING::execute()
{
  
  // retrieve data stored in the body
  // of the request

  const uint64_t obj_size = get_obj_size();

  if (obj_size == 0)
  {
    op_ret = -ERR_TOO_SMALL;
 
    sing_err = sing_err_name::BAD_PARAMS;
    return;
  }

  if(obj_size > s->cct->_conf->rgw_max_put_size)
  {
    op_ret = -ERR_TOO_LARGE;
   
    sing_err = sing_err_name::LARGE_ERR; // too large object

    return;
  }


  op_ret = store->check_quota(s->bucket_owner.get_id(), s->bucket,
                              user_quota, bucket_quota, obj_size);

  if(op_ret < 0)
  {

   ldout(s->cct, 20) << "checked_quota() returned ret=" 
                     << op_ret << dendl;
 
   sing_err = sing_err_name::QUOTA_ERR;

   op_ret = -ERR_QUOTA_EXCEEDED;
   return;
  }

  op_ret = store->check_bucket_shards(s->bucket_info, s->bucket, 
                                      bucket_quota);

  if(op_ret < 0)
  {

    sing_err = sing_err_name::QUOTA_ERR;

    ldout(s->cct, 20) << "check_bucket_shards() returned ret="
                      << op_ret << dendl; 

    return; 
  }
  
    
  // checked all the options
  s->op_type = get_op_->get_type(); 
  get_op_->pre_exec();
  get_op_->execute();

  s->op_type = RGWPutObj_ObjStore::get_type(); // reset type




  // tried to retrieve a manifest (might have failed)
 

  if(get_op_->manifest)
  { // succeeded in retrieving a manifest (data layout)
    op_ret = extend_manifest(*(get_op_->manifest), obj_size);
  }
  else
  { // create a new manifest (object does not exist)
    manifest_ = new RGWObjManifest();
    op_ret    = create_new_manifest(*manifest_, obj_size);
  }
   
}



int 
RGWPutObj_ObjStore_SING::init_processing()
{

  // first run the super class check in order
  // to make sure that we proceed further
  op_ret = RGWPutObj_ObjStore::init_processing();
  
  if(op_ret < 0)
  {
    sing_err = sing_err_name::INTERNAL_ERR;
    return op_ret; // something went wrong
  }


  // now we need to check if the bucket which is 
  // being accessed by the user exists or not
  //RGWObjectCtx  tmpCtx(store);
  RGWObjectCtx* tmpPtr = static_cast<RGWObjectCtx*>(s->obj_ctx);

  const int res = store->get_bucket_info(
                  *tmpPtr, 
                  s->bucket_tenant,
                  s->bucket_name, s->bucket_info,
                  nullptr, nullptr);

  if(res < 0 && res != -ENOENT)
  {

    sing_err = sing_err_name::ACL_ERR;
    op_ret = res; // set the error code and return
    return res; 
  }

  // check if need to create a bucket before processing further
  if(res == -ENOENT)
  {
    op_ret = create_bucket();
    if (op_ret  < 0)
    { // error occurred
    
      sing_err = sing_err_name::ACL_ERR;

      return op_ret;
    }
    else 
    { // retry to retrieve bucket info

      op_ret = store->get_bucket_info(
                              *tmpPtr, s->bucket_tenant,
                              s->bucket_name, s->bucket_info,
                              nullptr, nullptr);

      if(op_ret < 0)
      {
        sing_err = sing_err_name::INTERNAL_ERR;
        return op_ret;
      }

    }
  }
  

  bucket_exists = true; // the bucket must exist now

  return 0; // successfully created a bucket or retrieved
            // information about a bucket 

}


void
RGWPutObj_ObjStore_SING::do_error_response()
{
  const char* tranID = s->info.env->get("HTTP_X_TRAN_ID", nullptr);
  assert(tranID);
  
  dump_header(s, "X-Tran-Id", tranID);
  set_req_state_err(s, op_ret);
  dump_errno(s);


  s->formatter->open_object_section(""); // empty wrapper
  s->formatter->open_object_section("Result");
  s->formatter->dump_unsigned("Error_Type", sing_err);
  s->formatter->close_section(); // close 'Result'
  s->formatter->close_section(); // close JSON object

  // send the body
  end_header(s, nullptr, nullptr, NO_CONTENT_LENGTH, true, false);


}



int
RGWPutObj_ObjStore_SING::extend_manifest(RGWObjManifest& manifest,
                         const uint64_t write_size)
{

  // check if if it is possible to append any 
  // more data to the current head
  const bool append_head_only = (write_size <= (manifest.get_max_head_size() - manifest.get_head_size()));

  if(append_head_only)
  {
    //try to append to head
    rgw_raw_obj tmp_raw_obj;
    const bool res_conv = store->obj_to_raw(manifest.get_head_placement_rule(), manifest.get_obj(), &tmp_raw_obj);
  
    if(!res_conv)
    {
      // failed to convert onject to raw object
      derr << "ERROR: store->obj_to_raw failed" << dendl;
      sing_err = sing_err_name::INTERNAL_ERR;
      
      return -ERR_INVALID_OBJECT_NAME;
    }

    // successfully converted the head
    writeRados_.push_back(std::move(
      RGWPutObj_ObjStore_SING::SINGRadosObj(tmp_raw_obj, write_size,
                                      manifest.get_head_size(), 0)));

    // update head size and object size
    manifest.set_head_size(manifest.get_head_size() + write_size);
    manifest.set_obj_size(manifest.get_obj_size() + write_size);

    return 0; // success
  }

  // store the new object size
  const uint64_t new_total_obj_size = manifest.get_obj_size() + write_size;


  // success
  RGWObjManifestRule rule;
  const bool found = manifest.get_rule(0, &rule);

  if(!found)
  {
    derr << "ERROR: manifest->get_rule could not find rule" << dendl;
    sing_err = sing_err_name::INTERNAL_ERR;
    return -EIO;
  }
 
  
  // iterate over all current objects and 
  // get their objects
  if(!manifest.has_tail() || (manifest.get_head_size() == manifest.get_max_head_size()))
  {

    //try to append to head
    rgw_raw_obj tmp_raw_obj;
    const bool res_conv = store->obj_to_raw(manifest.get_head_placement_rule(), manifest.get_obj(), &tmp_raw_obj);
  
    if(!res_conv)
    {
      // failed to convert onject to raw object
      derr << "ERROR: store->obj_to_raw failed" << dendl;
      sing_err = sing_err_name::INTERNAL_ERR;
      
      return -ERR_INVALID_OBJECT_NAME;
    }

    // only head object
    writeRados_.push_back(std::move(
      RGWPutObj_ObjStore_SING::SINGRadosObj(tmp_raw_obj, 
      (manifest.get_max_head_size() - manifest.get_head_size()),
      manifest.get_head_size(), 0)));

   
    // need to create a few objects
    rgw_obj_select cur_obj;
    uint64_t cur_offset = manifest.get_max_head_size();
    uint64_t cur_stripe = (cur_offset - manifest.get_max_head_size()) / rule.stripe_max_size + 1;

    uint64_t written_bytes = (manifest.get_max_head_size() - manifest.get_head_size());

    while(written_bytes < write_size)
    {
      manifest.get_implicit_location(0, cur_stripe,
                                     cur_offset, nullptr,
                                     &cur_obj);


      // compute a raw object for writing
      writeRados_.push_back(std::move(
        RGWPutObj_ObjStore_SING::SINGRadosObj(cur_obj.get_raw_obj(store),
                                            rule.stripe_max_size, 0, 0)));


      if(writeRados_.back().obj_info.empty())
      {
        // clear all write objects
        writeRados_.clear();
    
        derr << "ERROR: cur_obj.get_raw_obj returned an empty objcet (extend_manifest)" << dendl;
       
        sing_err = sing_err_name::INTERNAL_ERR;
        return -EIO;
       
      }

      // update stripe number and offset
      ++cur_stripe;
      cur_offset += rule.stripe_max_size;
      written_bytes += rule.stripe_max_size;
   
    } // while
 
  }//if
  else
  { // there are some existing tail objects or head is full
    // compute append offset and size
    const uint64_t tail_size = manifest.get_obj_size() - manifest.get_max_head_size();

    const uint64_t app_off = tail_size - (tail_size / rule.stripe_max_size)*rule.stripe_max_size;
    
    uint64_t cur_stripe = (tail_size / rule.stripe_max_size) + 1;
    uint64_t cur_offset = tail_size;
    uint64_t written_bytes = 0;

    rgw_obj_select cur_obj;    

    if(app_off)
    { // need to retrieve current object and append to it
      manifest.get_implicit_location(0, cur_stripe, tail_size, 
                            nullptr, &cur_obj);

      writeRados_.push_back(std::move(
        RGWPutObj_ObjStore_SING::SINGRadosObj(cur_obj.get_raw_obj(store), 
                                        (rule.stripe_max_size - app_off),
                                                            app_off, 0)));

      if(writeRados_.back().obj_info.empty())
      {
        derr << "ERROR: extend_manifest, cannot append to tail" << dendl;
        writeRados_.clear();
        sing_err = sing_err_name::INTERNAL_ERR;
        return -EIO;
      }

      // update stripe number
      ++cur_stripe;
      cur_offset += (rule.stripe_max_size - app_off);
      written_bytes += (rule.stripe_max_size - app_off);
    }// if
    

    // just keep appending 
    // until write is complete
    while(written_bytes < write_size)
    {
      // create a new object for writing
      manifest.get_implicit_location(0, cur_stripe, cur_offset, 
                            nullptr, &cur_obj);

      writeRados_.push_back(std::move(
        RGWPutObj_ObjStore_SING::SINGRadosObj(cur_obj.get_raw_obj(store), 
                                                    rule.stripe_max_size,
                                                                  0, 1)));

      if(writeRados_.back().obj_info.empty())
      {
        derr << "ERROR: extend_manifest, cannot append to tail" << dendl;
        writeRados_.clear();
        sing_err = sing_err_name::INTERNAL_ERR;
        return -EIO;
      }

      // update stripe number
      ++cur_stripe;
      cur_offset += rule.stripe_max_size;
      written_bytes += rule.stripe_max_size;

    }// while


  }// else !has_tail()
   

  
  // update the current manifest
  manifest.set_obj_size(new_total_obj_size);
  manifest.set_head_size(manifest.get_max_head_size());

  return 0;
}



void
RGWPutObj_ObjStore_SING::do_send_response(const RGWObjManifest& manifest)

{

  const char* tranID = s->info.env->get("HTTP_X_TRAN_ID", nullptr);
  assert(tranID);
  
  dump_header(s, "X-Tran-Id", tranID);
  
  // no errors occurred
  // send a manifest
  set_req_state_err(s, SING_HTTP_OK);
  dump_errno(s);


  s->formatter->open_object_section("");

  // 'Result' field
  s->formatter->open_object_section("Result");

  // 'Data_Manifest' field
  s->formatter->open_object_section("Data_Manifest");  

  manifest.dump(s->formatter); // encode as a JSON object
  
  // close 'Data_Manifest' field
  s->formatter->close_section();


  // send object for writing to
  // Array 'Rados_Objs'
  s->formatter->open_array_section("Rados_Objs");
  
  for(const auto& rad_obj : writeRados_)
  {
    s->formatter->open_object_section("Object");
    // store object info
    rad_obj.obj_info.dump(s->formatter);
    s->formatter->dump_unsigned("size", rad_obj.size_);
    s->formatter->dump_unsigned("offset", rad_obj.offset_);
    s->formatter->dump_unsigned("new_object", rad_obj.new_write_);
    
    // Rados object has been completed
    s->formatter->close_section();

  } // for

 
  // close 'Rados_Objs' field
  s->formatter->close_section();

  // close 'Result' field
  s->formatter->close_section(); // close JSON object

  // close empty object (wrapper)
  s->formatter->close_section();


  // clear the write objects
  writeRados_.clear();


  // send the body
  end_header(s, nullptr, nullptr, NO_CONTENT_LENGTH, true, false);

}


int
RGWPutObj_ObjStore_SING::build_tail_path(const rgw_raw_obj& head_obj,
                              const string& tail_prefix,
                              string& tail_path)
{

  // Get the object path used for  
  // creating new Rados objects. 
  // The path contains bucket_marker, maybe_namespace, and then tail
  // prefix

  std::stringstream build_path; // path value
  const string& oid_val = head_obj.oid;

  auto pos_val = oid_val.find("_");
 
  if(pos_val == std::string::npos)
  { // undescore not found
  
    sing_err = sing_err_name::INTERNAL_ERR;
    return -ERR_INVALID_OBJECT_NAME;

  }
 
  // found the bucket marker
  // look for shadow namespace
  auto pos_ns = oid_val.find(SING_SHADOW_NS);

  if(pos_ns == std::string::npos)
  { // need to append the namespace
    build_path << std::move(oid_val.substr(0, pos_val)) 
               << SING_SHADOW_NS << tail_prefix;
  }
  else // it has been found in the string
  {
    decltype(oid_val.length()) end_idx = pos_ns + SING_SHADOW_NS.length();
   
    // read everything from 0 till end_idx
    build_path << std::move(oid_val.substr(0, end_idx))
               << tail_prefix;
 
  }


  

  // done building the path
  tail_path = std::move(build_path.str()); // return the string
  
  return 0;
}


void
RGWPutObj_ObjStore_SING::send_response()
{
 
  // check which version to use for sending a request to
  // the client

  // check if there was an error
  if(op_ret < 0 || sing_err != sing_err_name::SUCCESS)
  {
    do_error_response();
  }
  else
  {
    // send the manifest and explicit objects
    if(get_op_->manifest)
    {
      do_send_response(*(get_op_->manifest));
    }
    else
    { // send the newly created manifest
      do_send_response(*manifest_);
    }
  } // else (send response)
  
}


int
RGWPutObj_ObjStore_SING::prepare_init(uint64_t* chunk_size, const rgw_obj& obj)
{

  const int res_code = store->get_max_chunk_size(
            s->bucket_info.placement_rule,
            obj, chunk_size);

  if(res_code < 0)
  {
    return res_code;
  }


  return 0;
}


int
RGWPutObj_ObjStore_SING::init_manifest(CephContext* cct,
                     RGWObjManifest* man_ptr, 
                     const string& placement_rule, 
                     rgw_bucket& bucket, 
                     rgw_obj& obj)
{
  
  man_ptr->set_tail_placement(placement_rule, bucket);
  man_ptr->set_head(placement_rule, obj, 0);

  // generate a random string for storing the object
  
  char buf[33];
  gen_rand_alphanumeric(cct, buf, sizeof(buf) - sizeof(char));
  
  // use the passed string for
  // generting keys
  
  string oid_prefix(".");
  oid_prefix.append(buf);
  oid_prefix.append("_");

  /* TODO: Need to check if the generated object does not
     exist yet */

 
  man_ptr->set_prefix(oid_prefix);

  man_ptr->set_tail_instance(obj.key.instance);
  //man_ptr->set_obj_size(data_size); // initialize the size

 
  return 0;
}




int
RGWPutObj_ObjStore_SING::create_new_manifest(RGWObjManifest& manifest,
                         const uint64_t write_size)
{

  // create a new manifest according to object name, bucket
  // name and object size.
  


  rgw_obj head_obj; // create a head object  
  head_obj.init(s->bucket, s->object.name);
  head_obj.key.set_instance("");


  uint64_t max_chunk_size = 0;
  int res_code = prepare_init(&max_chunk_size, head_obj);

  if(res_code < 0)
  {
    op_ret = res_code;
    sing_err = sing_err_name::INTERNAL_ERR;
    return res_code;
  }

  // set placement rules of the manifest
  manifest.set_trivial_rule(max_chunk_size,
                            store->ctx()->_conf->rgw_obj_stripe_size);


  bool gen_again = false; // if the generated random
                          // prefix does not overlap with
                          // objects

  do
  {
    res_code = init_manifest(store->ctx(), &manifest,
                             s->bucket_info.placement_rule,
                             head_obj.bucket,
                             head_obj);
 
    if(res_code < 0)
    {
      // some internal error with buckets
      ldout(store->ctx(), 0) << "create_new_manifest: generaotr.create_begin"
                           << dendl;
      sing_err = sing_err_name::INTERNAL_ERR;
      return res_code;
    }



  } while(gen_again); // try creating manifest until no overlaps found
             

  
  // try to retrieve the same rule
  RGWObjManifestRule tmp_rule;
  const bool found = manifest.get_rule(0, &tmp_rule);

  if(!found)
  {
    derr << "ERROR: manifest.get_rule() could not find a rule" << dendl;
    
    sing_err = sing_err_name::INTERNAL_ERR;
    return -EIO;
  }

  // rule works
  const uint64_t stripe_size = tmp_rule.stripe_max_size;
 
    
  // generate objects
  rgw_obj_select cur_obj;



  // first write is to the head
  manifest.get_implicit_location(0, 0, 0, nullptr,
                                 &cur_obj);

  // get raw object
  rgw_raw_obj write_obj = cur_obj.get_raw_obj(store);

  if(write_obj.empty())
  {
    derr << "ERROR: cur_obj.get_raw_obj returned an empty object" << dendl;
    sing_err = sing_err_name::INTERNAL_ERR;
    return -EIO;
  }


  uint64_t cur_offset = (write_size < manifest.get_max_head_size())? write_size : manifest.get_max_head_size();

  // push the retrieved object onto the stack
  writeRados_.push_back(std::move(
    RGWPutObj_ObjStore_SING::SINGRadosObj(write_obj, cur_offset,
                                                         0, 1)));

  if(cur_offset == write_size)
  {
    manifest.set_obj_size(write_size);
    manifest.set_head_size(write_size); // head is larger
    return 0; // success
  }

  uint64_t cur_stripe = (cur_offset - manifest.get_max_head_size()) / stripe_size + 1;



  while(cur_offset < write_size)
  { // generate objects until no more objects needed
    manifest.get_implicit_location(0, cur_stripe, 
                                  cur_offset, nullptr,
                                  &cur_obj);

    // get raw object
    writeRados_.push_back(std::move(
      RGWPutObj_ObjStore_SING::SINGRadosObj(cur_obj.get_raw_obj(store),
                                                   stripe_size, 0, 1)));

    if(writeRados_.back().obj_info.empty())
    {
      // clear all write objects
      writeRados_.clear();

      derr << "ERROR: cur_obj.get_raw_obj returned an empty object (create_new_manifest)" << dendl;
      sing_err = sing_err_name::INTERNAL_ERR;
      return -EIO;
    }// if

    // update stripe number and offset
    ++cur_stripe;
    cur_offset += stripe_size; // increase the current offset

  }// while
  
  // got passed all values
  manifest.set_obj_size(write_size); 
  manifest.set_head_size(manifest.get_max_head_size());

  return 0; // successfully created a manifest

}


void
RGWPost_Manifest_SING::init(RGWRados* store, 
                            struct req_state* state,
                            RGWHandler* handler)
{

  RGWPostObj_ObjStore::init(store, state, handler);
}



void
RGWPost_Manifest_SING::execute()
{
  // update the metadata field
  if(!manifest || !obj_data_)
  {
    return; // don't do anything
  }


  // need to update the metadata 
  // by issueing the write_meta method of the RGWRados::Object::Write
  policy.create_default(s->user->user_id, s->user->display_name); // set the default policy 

  
  // make sure never write any data to the header object
  RGWObjectCtx& obj_ctx = *(static_cast<RGWObjectCtx*>(s->obj_ctx));

  obj_ctx.obj.set_atomic(manifest->get_obj_sing());
  RGWRados::Object op_target(store, s->bucket_info, *(static_cast<RGWObjectCtx*>(s->obj_ctx)), manifest->get_obj());
  RGWRados::Object::Write obj_op(&op_target);

  obj_op.meta.manifest     =  manifest;
  obj_op.meta.remove_objs  =  nullptr; // nothing to remove
  obj_op.meta.ptag         =  &s->req_id;
  obj_op.meta.owner        =  s->owner.get_id();
  obj_op.meta.flags        =  PUT_OBJ_CREATE;
  obj_op.meta.modify_tail  =  false; // NOT SURE about this (now no need since the worker modifies)
  obj_op.meta.completeMultipart = false; // no multipart
  

  // set some attributes
  std::map<string, bufferlist> attrs;
  bufferlist acl_bl; 
  policy.encode(acl_bl);
  attrs[RGW_ATTR_ACL] = acl_bl; //ACL attribute
  op_ret = obj_op.write_meta(0, obj_data_, attrs);

  if(op_ret < 0)
  {
    sing_err = sing_err_name::INTERNAL_ERR;
  } 
  
}

void
RGWPost_Manifest_SING::send_response()
{
  
  const char* tranID = s->info.env->get("HTTP_X_TRAN_ID", nullptr);
  assert(tranID);
  
  dump_header(s, "X-Tran-Id", tranID);
  
  if(sing_err == sing_err_name::SUCCESS)
  {
    set_req_state_err(s, SING_HTTP_OK);
    dump_errno(s);
  }
  
  else // soemthing went wrong
  {
    set_req_state_err(s, op_ret);
    dump_errno(s);
  }


  // empty JSOn object wrapper
  s->formatter->open_object_section("");

  // 'Result' field
  s->formatter->open_object_section("Result");
  s->formatter->dump_unsigned("Error_Type", sing_err);
  // close 'Result'
  s->formatter->close_section(); 


  s->formatter->close_section(); // close empty wrapper

  // send a JSON response to the client
  end_header(s, nullptr, nullptr, NO_CONTENT_LENGTH, true, false);
}


int
RGWPost_Manifest_SING::manifest_decoding(JSONObj* json_obj)
{

  // read the manifest from the JSON
  // and try to decode it
  // need to read the same order as formatter dumps

  
  sing_err = sing_err_name::BODY_TYPE_ERR;
  
  JSONObjIter objs_itr = json_obj->find_first("objs");
  
  if(objs_itr.end() || !(*objs_itr)->is_array())
  {
    
    return -ERR_NO_SUCH_UPLOAD;
  }


  unsigned long long tmp_ofs, tmp_loc_ofs, tmp_size;
  rgw_bucket  bucket_val;
  rgw_obj_key key_val;
  std::map<uint64_t, RGWObjManifestPart> obj_parts;

  // read the maps for the manifest
  for(; !objs_itr.end(); ++objs_itr)
  { // decode all the parts
    
    try
    { 
      JSONDecoder::decode_json("ofs", tmp_ofs, *objs_itr, true);
      auto part_itr = (*objs_itr)->find_first("part");
      auto loc_itr  = (*part_itr)->find_first("loc");
      
      // location
      auto bucket_itr = (*loc_itr)->find_first("bucket");
      bucket_val.decode_json(*bucket_itr);

      auto key_itr = (*loc_itr)->find_first("key");
      key_val.decode_json(*key_itr);

      // loc_ofs
      JSONDecoder::decode_json("loc_ofs", tmp_loc_ofs, *part_itr, true);
      JSONDecoder::decode_json("size", tmp_size, *part_itr, true);

      // create a part
      RGWObjManifestPart tmpPart;
      tmpPart.loc = std::move(rgw_obj(bucket_val, key_val));
      tmpPart.loc_ofs = static_cast<uint64_t>(tmp_loc_ofs);
      tmpPart.size    = static_cast<uint64_t>(tmp_size);

      // create a part
      obj_parts[static_cast<uint64_t>(tmp_ofs)] = std::move(tmpPart);
      

    } catch(JSONDecoder::err& e)
     {
       return -ERR_NO_SUCH_UPLOAD;
     }
    
  } // for (iterate over parts)

  // parts have been created
  unsigned long long man_obj_size, man_head_size, man_max_head_size;
  bool man_explicit_objs;
  std::string man_prefix, man_tail_instance, man_tail_placement_rule;
  rgw_bucket tail_bucket;
  std::map<uint64_t, RGWObjManifestRule> obj_rules;

  try // try to decode the rest of manifest
  {
   auto man_tail_plc_itr = json_obj->find_first("tail_placement");
   auto man_rules_itr    = json_obj->find_first("rules");

   // start decoding
   JSONDecoder::decode_json("obj_size", man_obj_size, json_obj, true);
   JSONDecoder::decode_json("explicit_objs", man_explicit_objs, json_obj, true);
   JSONDecoder::decode_json("head_size", man_head_size, json_obj, true);
   JSONDecoder::decode_json("max_head_size", man_max_head_size, json_obj, true);
   JSONDecoder::decode_json("prefix", man_prefix, json_obj, true);
   JSONDecoder::decode_json("tail_instance", man_tail_instance, json_obj, true);


  // decode the placement rule and manifest rules
  JSONDecoder::decode_json("placement_rule", man_tail_placement_rule,
                            *man_tail_plc_itr, true);

  // decode the tail bucket
  tail_bucket.decode_json(*((*man_tail_plc_itr)->find_first("bucket")));
  

  // final step is to decode the rules
  unsigned long long rule_map_idx, rule_start_ofs, 
                     rule_part_size, rule_stripe_max_size;

  unsigned long      rule_start_part_num;
  string rule_override_prefix;
  JSONObjIter rule_tmp_iter;
  JSONObjIter rule_tmp_key;
  JSONObjIter rule_tmp_val;  

  for(; !man_rules_itr.end(); ++man_rules_itr)
  {

    // find entry
    rule_tmp_iter = (*man_rules_itr)->find_first("entry");    
    
    // find key-value
    rule_tmp_key = (*rule_tmp_iter)->find_first("key");
    rule_tmp_val = (*rule_tmp_iter)->find_first("val");

    // decode map key
    ::decode_json_obj(rule_map_idx, *rule_tmp_key);
    
    // decode the value (rule)
    rule_tmp_iter = (*rule_tmp_val)->find_first("start_part_num");
    ::decode_json_obj(rule_start_part_num, *rule_tmp_iter);
    
    rule_tmp_iter = (*rule_tmp_val)->find_first("start_ofs");
    ::decode_json_obj(rule_start_ofs, *rule_tmp_iter);

    rule_tmp_iter = (*rule_tmp_val)->find_first("part_size");
    ::decode_json_obj(rule_part_size, *rule_tmp_iter);

    rule_tmp_iter = (*rule_tmp_val)->find_first("stripe_max_size");
    ::decode_json_obj(rule_stripe_max_size, *rule_tmp_iter);

    rule_tmp_iter = (*rule_tmp_val)->find_first("override_prefix");
    ::decode_json_obj(rule_override_prefix, *rule_tmp_iter);


    RGWObjManifestRule tmp_obj_rule(static_cast<uint32_t>(rule_start_part_num),
                                    static_cast<uint64_t>(rule_start_ofs),
                                    static_cast<uint64_t>(rule_part_size),
                                    static_cast<uint64_t>(rule_stripe_max_size));

    // copy efficiently the content
    tmp_obj_rule.override_prefix.swap(rule_override_prefix);

    // enocde the rule into the map
    obj_rules[static_cast<uint64_t>(rule_map_idx)] = std::move(tmp_obj_rule);
    
    
  } // for rules



  } catch (JSONDecoder::err& e)
  {
    return -ERR_NO_SUCH_UPLOAD;
  }
  

  // all values have been decoded
  // use them and initialize the manifest


  // create a manifest
  manifest = new RGWObjManifest_SING();
  assert(manifest);

  // now set the values
  // SING manifest methods
  manifest->set_explicit_sing(man_explicit_objs);
  manifest->set_objs_sing(std::move(obj_parts));
  manifest->set_rules_sing(std::move(obj_rules));

  // RGWObjmanifest methods
  manifest->set_tail_placement(man_tail_placement_rule, tail_bucket);
  manifest->set_prefix(man_prefix);
  manifest->set_head_size(static_cast<uint64_t>(man_head_size));
  manifest->set_obj_size(static_cast<uint64_t>(man_obj_size));
  manifest->set_max_head_size(static_cast<uint64_t>(man_max_head_size));
  manifest->set_tail_instance(man_tail_instance);


  // success
  sing_err = sing_err_name::SUCCESS;
  
  return 0;

}

int
RGWPost_Manifest_SING::decode_json_manifest()
{


  sing_err = sing_err_name::INTERNAL_ERR;

  if(s->content_length <=0)
  { 
    sing_err = sing_err_name::SMALL_ERR;
    return -ERR_TOO_SMALL; // no content
  }

  
  const size_t need_read = static_cast<size_t>(s->content_length);

  if(need_read > MAX_INT_VAL)
  {
    sing_err = sing_err_name::LARGE_ERR;

    return -ERR_TOO_LARGE; // too big content
  }  


  int res_code = -ERR_NO_SUCH_UPLOAD; // response code

  size_t left_read = need_read;

  char* json_buffer = new char[need_read]; // need to read

  size_t read_bytes = 0;
  int len = 0;

  while(left_read)
  {
    len = recv_body(s, (json_buffer+read_bytes), left_read);

    if(len < 0)
    {
      delete[] json_buffer;
      return res_code;
    }
   
    // cast int to size_t
    const size_t got_data = static_cast<const size_t>(len);


    // update read for pointers
    read_bytes += got_data;
    if(got_data > left_read)
    {
      break;
    }

    left_read -= got_data;

  }// while
  

  // decode the char array into a JSON object.
  JSONParser parser;
  
  bool ret = parser.parse(json_buffer, static_cast<int>(need_read));
  
  if(!ret)
  {
    delete[] json_buffer;
    return res_code;
  }


  auto iter = parser.find_first("Result"); // find a JSON
                                           // object that 
                                           // encapsultes 
                                           // the manifest
  if(iter.end())
  { // did not find
    delete[] json_buffer;
    return res_code;
  }

  auto obj_mani = (*iter)->find_first("Data_Manifest"); // look for 
                                                       // manifest

  if(obj_mani.end())
  {
    delete[] json_buffer;
    return res_code;
  }

  

  res_code = manifest_decoding(*obj_mani);
  
  if(res_code < 0)
  {
    delete[] json_buffer;

    return res_code;
  }

  // decode the object field of the manifest
  rgw_obj_key headKey;
  rgw_bucket  headBucket;

  // 'Head_Object' field

  auto head_iter = (*iter)->find_first("Head_Object");
  
  // try to decode
  try
  {
    headBucket.decode_json(*((*head_iter)->find_first("bucket")));
    headKey.decode_json(*((*head_iter)->find_first("key")));

  } catch(JSONDecoder::err& exp)
  {
    res_code = -ERR_NO_SUCH_UPLOAD;
    delete[] json_buffer;
  }
   

  if(res_code < 0)
  {
    return res_code;
  }
  

  // set the object field to the decoded one
  uint64_t tmp_head_size = manifest->get_head_size();
  const string&  tmp_head_placement = manifest->get_tail_placement().placement_rule;
  manifest->set_head(tmp_head_placement, rgw_obj(headBucket, headKey), tmp_head_size);


  // get the old value of the data
  unsigned long long completed_data = 0;  

  try
  {
    JSONDecoder::decode_json("Data_offset", completed_data, *iter, true);
  } catch(JSONDecoder::err& exp)
  {
    res_code = -ERR_NO_SUCH_UPLOAD;
    delete[] json_buffer;
  }

  if(res_code < 0)
  {
    return res_code;
  }

  // calculate the written data size
  const uint64_t read_size_val = static_cast<const uint64_t>(completed_data);
  obj_data_ = (read_size_val < manifest->get_obj_size()) ?\
                              (manifest->get_obj_size() - read_size_val):\
                              0;

  if(!obj_data_)
  {
    sing_err = sing_err_name::SMALL_ERR;
    res_code = -ERR_NO_SUCH_UPLOAD;
    
    delete[] json_buffer;

    return res_code;
  }

  /******** DONE Decoding the Received Manifest ***********/
  
  // no errors
  sing_err = sing_err_name::SUCCESS;
  res_code = 0; 
  delete[] json_buffer;
  
  return res_code;  


}



int
RGWPost_Manifest_SING::init_processing()
{

  int res_code = RGWPostObj_ObjStore::init_processing();

  if(res_code < 0)
  {
    sing_err = sing_err_name::INTERNAL_ERR;
    
    return res_code;
  }


  // read the body and try to decode into 
  // a JSON object
  
  const char* content_type = s->info.env->get("CONTENT_TYPE", nullptr);

  if(!content_type)
  {
    sing_err = sing_err_name::BODY_TYPE_ERR;
    op_ret = -ERR_INVALID_UTF8;

    return -ERR_INVALID_UTF8;
 
  }


  // HTTP Headers use ASCII encoding
  std::string  type_str(content_type);
  std::transform(type_str.begin(), type_str.end(), type_str.begin(),
                 [] (unsigned char c) { return std::tolower(c); }); // transform to lower case first

  auto find_json = type_str.find("json");

  if(find_json == std::string::npos)
  {
    sing_err = sing_err_name::BODY_TYPE_ERR;
    op_ret = -ERR_INVALID_UTF8;

    return -ERR_INVALID_UTF8;
  }

  // read the body and get JSON object
  res_code = decode_json_manifest();  
  
  if(res_code < 0)
  {
    op_ret = res_code;
    return res_code;
  }
  

  // successfully initialized the manifest 
  return 0;

}



/********************* RGWOps End ***********************/



RGWOp*
RGWHandler_REST_Obj_SING::op_delete()
{
  return new RGWDeleteObj_ObjStore_SING; 
}


RGWOp*
RGWHandler_REST_Obj_SING::op_get()
{
  return new RGWGetObjLayout_SING;
}


RGWOp*
RGWHandler_REST_Obj_SING::op_put()
{
  return new RGWPutObj_ObjStore_SING;
}

RGWOp*
RGWHandler_REST_Obj_SING::op_post()
{
  return new RGWPost_Manifest_SING;
}



RGWHandler_REST* 
RGWRESTMgr_SING_Info::get_handler(
  struct req_state* const s,
  const rgw::auth::StrategyRegistry& auth_registry,
  const std::string& frontend_prefix)
{
  s->prot_flags |= RGW_REST_SWIFT;
  //const auto& auth_strategy = auth_registry.get_sing();

  return nullptr;
  //return new RGWHandler_REST_SING_Info(auth_strategy);
}
