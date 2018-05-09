### Python Distributed Storage APIs 

##### Python Remote Data Package (singstorage)
The Python package contains python interfaces for reading/writing data from a distributed storage system.
The very first design only provides a few basic functions for the users. It can be extended in the future.



  
- \# **(Required)** Function takes a 'username' and a 'password' to   
  \#                authenticate the user with the storage system.  
  \#  
  \# Parameters:  
  \# - **username[in]**: cloud username  
  \# - **password[in]**: cloud password  
  \#  
  \#  **return**: singstorage.SUCCESS on success, failure_code on failure  
  ```python
  singstorage.connect(username, password)
  ```
 
- \# **(Required)** For writing Python bytearray or array
  \#                to a distributed storage system
  \#                (the passed object must support 
  \#                 the Python buffer protocol).  
  \#  
  \# Parameters:  
  \# - **path[in]**: a logical path used for identifying the place where
  \#                 data has to be written to (DATA NAME)  
  \# - **data[in]**: Python object that supports the Python
  \#                 buffer protocol
  \#    
  \#  **throw exception**: an exception (singstorage.IOException) 
  \#                       is thrown if the operation fails.  
  ```python
  singstorage.write_data(path, data)
  ```
  
  
- \# **(Required)** For reading/accesing data from 
  \#                a distributed storage system.  
  \#  
  \# Parameters:  
  \# - **path[in]**: a data path used for identifying 
  \#                 the place where data is read from 
  \#                 (DATA NAME)  
  \#   
  \#  **return**:   an iterable object which returns byte
  \#                by byte of data until the Python
  \#                StopIteration exception is raised.
  \#                May raise a singstorage.IOException
  \#                if some intermediate data cannot
  \#                be retrieved while iterating.
  \#
  \#  **throw exception**: an exception (singsotrage.IOException) 
  \#                       may be thrown on failed operation.  
  ```python
  singstorage.read_data(path)
  ``` 
  
- \# **(Required)** Close the communication session and 
  \#                  release resources.  
  \#  
  \# Parameters:   
  ```python
  singstorage.close()
  ```
 
