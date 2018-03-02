# Storage APIs for AI Cloud Storage

### Session

 - \\\* Initialize a *session* for a server to connect and write/read data from a Ceph storage cluster.  
     \*  
     \* Parameters:  
     \* - **username [in]**: cloud user name;  
     \* - **password [in]**: cloud user password;  
     \*  
     \* **return**  : 0 on success; errno on failure.   
     \*\\  
int **Session::init**(const char* username, const char* password);    
 
   
- \\\* Connect to a Ceph cluster for data reading/writing.  
   \*   
   \* Parameters:  
   \* - **auth_server_ip_addr [in]**: service server IPv4 address (e.g., "192.168.1.100:8000");   
   \*  
   \* **return**: 0 on success; errno on failure.   
   \*\\        
int **Session::connect**(const char* auth_server_ip_addr);  
  
-  \\\* Write **data** synchronously/asynchnosously to the storage cluster.  
   \*   
   \* Parameters:  
   \* - **data [in]**: data buffer for writing;  
   \* - **data_size [in]**: number of bytes to write;  
   \* - **bucket_name [in]**: unique bucket name to which write the data object;  
   \* - **object_name [in]**: unique object name for storing data in the cluster;  
   \* - **handle [out]**:  handle for writing data asynchronously;  
   \*  
   \* **return**: 0 on success; errno on failure.   
   \*\\   
int **Session::write_data_sync**(char* data, const size_t data_size, const char* bucket_name, const char* object_name);  
int **Session::write_data_async**(char* data, const size_t data_size, const char* bucket_name,  const char* object_name, aioHandle* handle);  

- \\\* Write **metadata** of an object sunchronously/asynchronously.  
   \*  
   \* Parameters:  
   \* - **key_value [in]**: metadata name and value pair map;  
   \* - **pairs_written [out]**: how many metadata pairs will be written after the method completes;  
   \* - **handle [out]**: handle for writing metadata asynchronously;  
   \*  
   \* **return**: 0 on success; errno on failure.    
   \*\\  
int **Session::write_meta_sync**(map<string, string> key_value, unsigned int* pairs_written);  
int **Session::write_meta_async**(map<string, string> key_value, unsigned int* pairs_written, aioHandle* handle);

- \\\* Read an object synchronously/asynchronously from the storage cluster.  
   \*   
   \* Parameters:
   \* - **bucket_name [in]**: unique bucket name which contains the data object;  
   \* - **object_name [in]**: unique object name which has to be read from the storagfe cluster;  
   \* - **buffer [out]**: buffer for reading the object into;  
   \* - **buf_len [in]**: buffer size;  
   \* - **handle [out]**: handle for asynchronous object reading;  
   \*  
   \* **return**: 0 on success; errno on failure.    
   \*\\  
int **Session::read_sync**(const char* bucket_name, const char* object_name,  char* buffer, size_t buf_len);  
int **Session::read_async**(const char* bucket_name, const char* object_name, char* buffer, size_t buf_len, aioHandle* handle);  

- \\\* Append data to an existing object.  
   \*  
   \* Parameters:  
   \* - **data [in]**: data to be appended to an existing object;    
   \* - **data_size [in]**: size of data to append;  
   \* - **bucket_name [in]**: unique bucket name which contains the data object;  
   \* - **object_name [in]**: unique object name which data will be appended to;  
   \* - **handle [out]**: handle for appending data asynchronously;  
   \*  
   \* **return**: 0 on success; errno on failure. 
   \*\\  
int **Session::append_data_sync**(char* data, const size_t data_size, const char* bucket_name, const char* object_name);    
int **Session::append_data_async**(char* data, const size_t data_size, const char* bucket_name,  const char* object_name, aioHandle* handle);  

- \\\* Close data session and release all allocated resources. This method also reads/writes all pending data   
   \* and releases communication resources.    
   \*  
   \* Parameters:   
   \*  
   \* **return**: 0 on success; errno on failure.  
   \*\\  
int **Session::close**();

**TODO:**

​	define ``aioHandle``, similar to librados ``AioCompletion``



### Buckets:(future APIs)

- \\\*  A class for handling administrative matters such as creating buckets,
    \*  setting ACLs, retrieving ACLs, and etc.  
    \*   
    \* Parameters:  
    \* - **username [in]**: cloud username;  
    \* - **password [in]**: cloud password;  
    \*  
    \* **return**: 0 on success; errno on failure.  
    \*\\  
int **AdminOps::init**(const char* username, const char* password);  

- \\\* Create a new bucket with a unique name and set its ACLs to ACLs value.  
    \*  
    \* Parameters:  
    \* - **bucket_name [in]**: unique bucket name which will be used for creating a new bucket;  
    \* - **ACLs [in]**: initialial ACLs of the newly created bucket;  
    \*  
    \* **return**: 0 on success; errno on failure.  
    \*\\  
int **AdminOps::create_bucket**(const char* bucket_name, ACL_t ACLs);

- \\\* Delete the bucket whose name is 'bucket_name'. The bucket must be empty. 
    \*
    \* Parameters:  
    \* - **bucket_name**: unique bucket name used to delete a storage container;    
    \*  
    \* **return**: 0 on success; errno on failure.  
    \*\\  
int **AdminOps::delete_bucket**(const char* bucker_name);

- \\\* Close an administrative session to the storage cluster. All allocated resources are released.  
    \*  
    \* Parameters:  
    \*  
    \* **return**: 0 on success; errno on failure.  
    \*\\  
int **AdminOps::close**();
