// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#ifndef CEPH_RGW_REST_SING_H
#define CEPH_RGW_REST_SING_H
#define TIME_BUF_SIZE 128

#include <boost/optional.hpp>
#include <boost/utility/typed_in_place_factory.hpp>

#include "../rgw_op.h"
#include "../rgw_rest.h"
#include "rgw_sing_auth.h"
#include "../rgw_http_errors.h"

#include <boost/utility/string_ref.hpp>

class RGWHandler_REST_SING : public RGWHandler_REST {
  friend class RGWRESTMgr_SING;
  friend class RGWRESTMgr_SING_Info;

  static constexpr const std::string STORAGE_PREFIX("sing");  

protected:
  const rgw::auth::Strategy& auth_strategy;

  virtual bool is_acl_op() {
    return false;
  }

  static int init_from_header(struct req_state* s,
                              const std::string& frontend_prefix);
public:

  RGWHandler_REST_SING(const rgw::auth::Strategy& auth_strategy)
    : auth_strategy(auth_strategy) {
  }
  ~RGWHandler_REST_SING() override = default;

  int validate_bucket_name(const string& bucket);

  int init(RGWRados *store, struct req_state *s, rgw::io::BasicClient *cio) override;
  int authorize() override;
  int postauth_init() override;

  RGWAccessControlPolicy *alloc_policy() { return nullptr; /* return new RGWAccessControlPolicy_SING; */ }
  void free_policy(RGWAccessControlPolicy *policy) { delete policy; }
};

class RGWRESTMgr_SING : public RGWRESTMgr {
protected:
  RGWRESTMgr* get_resource_mgr_as_default(struct req_state* const s,
                                          const std::string& uri,
                                          std::string* const out_uri) override {
    return this->get_resource_mgr(s, uri, out_uri);
  }

public:
  RGWRESTMgr_SING() = default;
  ~RGWRESTMgr_SING() override = default;

  RGWHandler_REST *get_handler(struct req_state *s,
                               const rgw::auth::StrategyRegistry& auth_registry,
                               const std::string& frontend_prefix) override;
};



class RGWHandler_REST_Bucket_SING : public RGWHandler_REST_SING
{
  protected:
    bool is_obj_update_op() override
    {
      return (s->op == OP_POST);
    }

  
    //  operations provided by the bucket handler class  
    RGWOp *get_obj_op(bool get_data);
    RGWOp *op_get() override;
    RGWOp *op_head() override;
    RGWOp *op_put() override;
    RGWOp *op_delete() override;
    RGWOp *op_post() override;
    RGWOp *op_options() override;


  public:
    using RGWHandler_REST_SING::RGWHandler_REST_SING;
    ~RGWHandler_REST_Bucket_SING() override = default;


    int init(RGWRados* const store, struct req_state* const state,
             rgw::io::BasicClient* const cio) override 
    {
      return RGWHandler_REST_SING::init(store, state, cio);
    }


}; // class RGWHandler_REST_Bucket_SING



class RGWHandler_REST_Obj_SING : public RGWHandler_REST_SING
{

  protected:
    bool is_obj_update_op() override
    {
      return (s->op == OP_POST);
    }

  
    //  operations provided by the bucket handler class  
    RGWOp *get_obj_op(bool get_data) { return nullptr; };
    RGWOp *op_get() override;
    RGWOp *op_head() override { return nullptr; };
    RGWOp *op_put() override;
    RGWOp *op_delete() override;
    RGWOp *op_post() override { return nullptr; };
    RGWOp *op_options() override { return nullptr;};


  public:
    using RGWHandler_REST_SING::RGWHandler_REST_SING;

    ~RGWHandler_REST_Obj_SING() override = default;

    int init(RGWRados* const store, struct req_state* const state,
             rgw::io::BasicClient* const cio) override
    {
      return RGWHandler_REST_SING::init(store, state, cio);
    }
 


}; // class RGWHandler_REST_Obj_SING


class RGWHandler_REST_SING_Info : public RGWHandler_REST_SING {
public:
  using RGWHandler_REST_SING::RGWHandler_REST_SING;
  ~RGWHandler_REST_SING_Info() override {}

  RGWOp *op_get() override {
    //return new RGWInfo_ObjStore_SING();
    return nullptr;
  }

  int init(RGWRados* const store,
           struct req_state* const state,
           rgw::io::BasicClient* const cio) override {
    state->dialect = "sing";
    state->formatter = new JSONFormatter;
    state->format = RGW_FORMAT_JSON;

    return RGWHandler::init(store, state, cio);
  }

  int authorize() override {
    return 0;
  }

  int postauth_init() override {
    return 0;
  }

  int read_permissions(RGWOp *) override {
    return 0;
  }
};

class RGWRESTMgr_SING_Info : public RGWRESTMgr {
public:
  RGWRESTMgr_SING_Info() = default;
  ~RGWRESTMgr_SING_Info() override = default;

  RGWHandler_REST *get_handler(struct req_state* s,
                               const rgw::auth::StrategyRegistry& auth_registry,
                               const std::string& frontend_prefix) override;
};



/**
 * The following classes are used for creating/deleting
 * buckets and object. We only operate on the object
 * heads since the clients are supposed to uppload data.
 */


class RGWPutObj_ObjStore_SING; // forward declaration


class RGWDeleteObj_ObjStore_SING : public RGWDeleteObj_ObjStore
{

  public:
    RGWDeleteObj_ObjStore_SING() {}
    ~RGWDeleteObj_Objstore_SING() override {}

    int verify_permission() override;
    int get_params() override;
    bool need_object_expiration() override { return true; }
    void send_response() override;

}; // class RGWDeleteObj_ObjStore


class RGWGetObjLayout_SING : public RGWGetObjLayout
{

  private:
    friend class RGWPutObj_ObjStore_SING; // for accessing state
    static uint64_t get_sing_error(const int sys_error);

  public:
    RGWGetObjLayout_SING() = default;
    ~RGWGetObjLayout_SING() override {}

    void send_response() override; // send a JSON
                                   // containing
                                   // object layout

}; // class RGWGetObjLayout_SING


class RGWPutObj_ObjStore_SING : public RGWPutObj_ObjStore
{
  private:
  
    /** Private handlers for creating/checking buckets
     *  and objects.
     */ 
    class SING_CreateBucket : public RGWCreateBucket_ObjStore
    {
      protected:
        bool need_metadata_upload() const override { return true;}
      
      public:
        SING_CreateBucket() {}
        ~SING_CreateBucket() override {}

        int get_params() override { return 0;};
        void send_response() override {}
    }; // class SING_CreateBucket    


   // Pointers to the worker operation
   RGWGetObjLayout_SING* get_op_ = nullptr; // get operation
   bool bucket_exists            = false;   // does bucket exist?   
      
 
   void do_empty_response();       // send an empty manifest to
                                   // the client
  
   uint64_t get_obj_size();        // retrieve object size
                                   // to be written
 
   void create_temp_manifest() const {} // create and initialize
                                        // manifest for user

   int create_bucket() const; // try to create a bucket
                              // if the required bucket does
                              // not exist.

    
  public:
    RGWPutObj_ObjStore_SING() {}

    ~RGPPutObj_ObjStore_SING() override
     {
       // release allocated resources

       if(get_op_)
       {
         delete get_op_;
         det_op_ = nullptr;
       }

     }    


    int verify_permission() override;
    void init(RGWRados* store, struct req_state* state,
              RGWHandler* dialect_handler) override;
   

    void execute() override;
    void pre_exec() override;

    int init_processing() override;   
    int get_params() override { return 0; }

    void send_response() override;
    int get_data(bufferlist& bl) override { return 0; }
     

}; // class RGWPutObj_ObjStore_SING


#endif
