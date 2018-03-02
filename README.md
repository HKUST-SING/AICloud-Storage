# Storage APIs for AI Cloud Storage

###Session

- int **Session::init**(char* username, char* passwords);
- int **Session::connect**(char* auth_server_ip_addr);
- int **Session::write_data_sync**(char* data, const size_t data_size, const char* bucket_name, const char* object_name);
- int **Session::write_data_async**(char* data, const size_t data_size, const char* bucket_name,  const char* object_name, aioHandle* handle);
- int **Session::write_meta_sync**(map<string, string> key_value, unsigned int* pairs_written);
- int **Session::write_meta_async**(map<string, string> key_value, unsigned int* pairs_written, aioHandle* handle);
- int **Session::read_sync**(char* bucket_name, char* object_name, char* buffer, size_t buf_len);
- int **Session::read_async**(char* bucket_name, char* object_name, char* buffer, size_t buf_len, aioHandle* handle);
- int **Session::append_data_sync**(char* data, const size_t data_size, const char* bucket_name, const char* object_name);
- int **Session::append_data_async**(char* data, const size_t data_size, const char* bucket_name,  const char* object_name, aioHandle* handle);
- int **Session::close**();

**TODO:**

​	define ``aioHandle``, similar to librados ``AioCompletion``



###Buckets:(to be continue)

- int **AdminOp::init**(char* username, char* passwords);
- int **AdminOp::create_bucket**(char* bucket_name, ACL_t ACLs);
- int **AdminOp::delete_bucket**(char* bucker_name);
- int **AdminOp::change_ACL**(char* bucket_name, ACL_t new_ACLS);
- int **AdminOp::close**();