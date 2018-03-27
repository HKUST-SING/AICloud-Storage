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

  static constexpr const std::string STORAGE_PREFIX = "sing";

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



class RGWHandler_REST_Obj_SING : public RGHandler_REST_SING
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
  ~RGWHandler_REST_SING_Info() override = default;

  RGWOp *op_get() override {
    return new RGWInfo_ObjStore_SING();
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

#endif
