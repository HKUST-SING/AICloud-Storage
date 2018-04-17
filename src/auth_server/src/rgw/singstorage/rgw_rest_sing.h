// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#ifndef CEPH_RGW_REST_SING_H
#define CEPH_RGW_REST_SING_H


#include <utility>

#include <boost/optional.hpp>
#include <boost/utility/typed_in_place_factory.hpp>

#include "../rgw_op.h"
#include "../rgw_rest.h"
#include "../rgw_http_errors.h"
#include "../rgw_common.h"


#include "rgw_sing_auth.h"
#include "rgw_sing_error_code.h"


#include <boost/utility/string_ref.hpp>

class RGWHandler_REST_SING : public RGWHandler_REST {
  friend class RGWRESTMgr_SING;
  friend class RGWRESTMgr_SING_Info;

  static const std::string STORAGE_PREFIX;  

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
    RGWOp *op_post() override;
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

}; // class RGWHandler_REST



/**
 * The following classes are used for creating/deleting
 * buckets and object. We only operate on the object
 * heads since the clients are supposed to uppload data.
 *
 * NOTICE: Since head objects are deleted every time a new
 *         operation on the object is executed 
 *         (see: RGWRados::Object::Write::write_meta),
 *         for now we must ensure that we never
 *         write raw data in the head object.
 */


class RGWPutObj_ObjStore_SING; // forward declaration


class RGWDeleteObj_ObjStore_SING : public RGWDeleteObj_ObjStore
{

  public:
    RGWDeleteObj_ObjStore_SING()
    : RGWDeleteObj_ObjStore()
    {}
    ~RGWDeleteObj_ObjStore_SING() override {}

    int verify_permission() override;
    int get_params() override;
    bool need_object_expiration() override { return true; }
    void send_response() override;

}; // class RGWDeleteObj_ObjStore


class RGWGetObjLayout_SING : public RGWGetObjLayout
{

  private:
   
    typedef struct ReadObjInfo
    {
      rgw_raw_obj obj_info;
      uint64_t    obj_size;

      ReadObjInfo(const rgw_raw_obj& raw_obj, 
                  const uint64_t stripe_size)
      : obj_info(raw_obj.pool, raw_obj.oid, raw_obj.loc),
        obj_size(stripe_size)
      {}


      ReadObjInfo(const struct ReadObjInfo& other)
      : obj_info(other.obj_info.pool, 
                 other.obj_info.oid, 
                 other.obj_info.loc),
        obj_size(other.obj_size)
      {}

      ReadObjInfo(struct ReadObjInfo&& other)
      : obj_size(other.obj_size)
      {
        // steal Rados info from the 'other' object
        obj_info.pool.name = std::move(other.pool.name);
        obj_info.pool.ns   = std::move(other.pool.ns);
        obj_info.oid       = std::move(other.oid);
        obj_info.loc       = std::move(other.loc);
      }



      ~ReadObjInfo() = default;

    } ReadObjInfo; // struct ReadObjInfo 

    std::vector<ReadObjInfo> rawObjs_; // all raw objects to read


    friend class RGWPutObj_ObjStore_SING; // for accessing state
    static uint64_t get_sing_error(const int sys_error);

    


  public:
    RGWGetObjLayout_SING() = default;
    ~RGWGetObjLayout_SING() override {}

    int verify_permission() override;
    
    void execute() override;

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

        int get_params() override 
        {
          policy.create_default(s->user->user_id, s->user->display_name);
          return 0;
        }

        void send_response() override {}
    }; // class SING_CreateBucket   

   
    typedef struct SINGRadosObj
    {
      rgw_raw_obj obj_info;
      uint64_t    size_;
      uint64_t    offset_;
      uint64_t    new_write_; // if true (==1), write, not append

   
      SINGRadosObj(const rgw_raw_obj& raw_obj,
                   const uint64_t write_av = 0,
                   const uint64_t offset = 0,
                   const uint64_t new_write = 0)
      : obj_info(raw_obj.pool, raw_obj.oid, raw_obj.loc),
        size_(write_av),
        offset_(offset),
        new_write_(new_write)
      {}


     SINGRadosObj(const struct SINGRadosObj& other)
     : obj_info(other.obj_info.pool, 
                other.obj_info.oid,
                other.obj_info.loc),
       size_(other.size_),
       offset_(other.offset_),
       new_write_(other.new_write_)
     {}


     SINGRadosObj(struct SINGRadosObj&& other)
     : obj_info(),
       size_(other.size_),
       offset_(other.offset_),
       new_write_(other.new_write_)
     {
       // steal Rados info from the 'other' object
       obj_info.pool.name = std::move(other.pool.name);
       obj_info.pool.ns   = std::move(other.pool.ns);
       obj_info.oid       = std::move(other.oid);
       obj_info.loc       = std::move(other.loc);

     }
 


     ~SINGRadosObj() = default;


    } SINGRadosObj; // struct SINGRadosObj

   std::vector<SINGRadosObj> writeRados_; // objects to be written to


 
   RGWObjManifest*   manifest_{nullptr}; // local manifest
   uint64_t data_size = 0; // data size to store in
                           // the cluster

   // Pointers to the worker operation
   RGWGetObjLayout_SING* get_op_{nullptr}; // get operation
   bool bucket_exists{false};   // does bucket exist? 

   using sing_err_name   = rgw::singstorage::SINGErrorCode; 

 
   uint64_t sing_err{sing_err_name::SUCCESS}; // SING specific error 
   

   void do_error_response();       // send an error code

   int extend_manifest(RGWObjManifest& manifest,
                       const uint64_t write_size);         
                                   // means the object
                                   // already exists;
                                   // need to extend the manifest

   int create_new_manifest(RGWObjManifest& manifest,
                           const uint64_t write_size);     
                                   // new object ==> new manifest

   int prepare_init(uint64_t* chunk_size, const rgw_obj& obj);
   int build_tail_path(const rgw_raw_obj& write_obj, 
                        const string& tail_prefix,
                        string& tail_path);

   int init_manifest(CephContext* cct, RGWObjManifest* man_ptr,
                     const string& placement_rule,
                     rgw_bucket& bucket,
                     rgw_obj& obj);

   // send the manifest to the worker
   void do_send_response(const RGWObjManifest& manifest); 
  
  
   uint64_t get_obj_size();        // retrieve object size
                                   // to be written
 

   int create_bucket();       // try to create a bucket
                              // if the required bucket does
                              // not exist.

   

    
  public:
    RGWPutObj_ObjStore_SING() 
    : RGWPutObj_ObjStore() 
    {}

    ~RGWPutObj_ObjStore_SING() override
     {
       // release allocated resources

       if(get_op_)
       {
         delete get_op_;
         get_op_ = nullptr;
       }
     
       if(manifest_)
       {
         delete manifest_;
         manifest_ = nullptr;
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



class RGWPost_Manifest_SING : public RGWPostObj_ObjStore
{

  private:
    static const std::string EMPTY_STRING;
   
    using sing_err_name = rgw::singstorage::SINGErrorCode; 


    // manifest for writing to the cluster
    // ingerited version used for accessing internal
    // manifest structures since they are 'protected'
    class RGWObjManifest_SING : public RGWObjManifest
    {

      public:
        RGWObjManifest_SING() 
        : RGWObjManifest()
        {}

        RGWObjManifest_SING(const RGWObjManifest_SING& other)
        : RGWObjManifest()
        {
          *this = other;
        }

        RGWObjManifest_SING& operator=(const RGWObjManifest_SING& other)
        {
          // use pointers in order to call
          // the assignment operator of the super class
          RGWObjManifest* parent_ptr = static_cast<RGWObjManifest*>(this);
          const RGWObjManifest* other_ptr  = static_cast<const RGWObjManifest*>(&other);
          
         // use the assignment operator of the parent
         *parent_ptr = *other_ptr;

          return *this;
        }

        ~RGWObjManifest_SING() = default; 

        void set_explicit_sing(const bool ex_val)
        {
          explicit_objs = ex_val;
        }       

        void set_objs_sing(std::map<uint64_t, RGWObjManifestPart>&& others)
        { 
          objs = std::move(others);
        }

       void set_rules_sing(std::map<uint64_t, RGWObjManifestRule>&& others)
       {
         rules = std::move(others);
       }

       rgw_obj& get_obj_sing() 
       {
         return obj;
       } 

    }; // class RGWPbjManifest_SING
    RGWObjManifest_SING* manifest{nullptr};    

    // error code
    uint64_t sing_err{sing_err_name::SUCCESS};

    // data written to the cluster
    uint64_t obj_data_{0};
  
    int decode_json_manifest();
    int manifest_decoding(JSONObj* jitr);


  protected:
    std::string get_current_filename() const override
    {
      return RGWPost_Manifest_SING::EMPTY_STRING;
    }

   std::string get_current_content_type() const override
   {
     return RGWPost_Manifest_SING::EMPTY_STRING;
   }

   bool is_next_file_to_upload() override 
   {
     return false;
   }

  public:
  
    RGWPost_Manifest_SING()
    : RGWPostObj_ObjStore()
    {}

    ~RGWPost_Manifest_SING() override
    {
      if (manifest)
      {
        delete manifest;
        manifest = nullptr;
      }
    }

    int verify_permission() override { return 0;}
    int get_params() override { return 0; } 
    int get_data(ceph::bufferlist& bl, bool&again) { return 0;}
    void init(RGWRados* store, struct req_state* state, 
              RGWHandler* handler) override;


    int init_processing() override;

    void send_response() override;
    void pre_exec() override { RGWPostObj_ObjStore::pre_exec();}
    void execute() override;
  

}; // class RGWPost_Manifest_SING

#endif
