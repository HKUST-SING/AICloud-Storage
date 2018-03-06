### Python Distributed Storage APIs 

##### Python Remote Data Package (sing_data)
The Python package contains python interfaces for reading/writing data from a distributed storage system.
The very first design only provides a few basic functions for the users. It can be extended in the future.


- \# **(Optional)** Function can be used for changing the raw data storage proeprties.  
  \# A map is taken as an input to update current settings. 
  \#  
  \# Examples: encoding='UTF-8', compression=None  
  ```python
  singstorage.set_properties(**kwargs)
  ```
  
- \# **(Required)** Function takes a 'username' and a 'password' to authenticate the user  
  \# with the storage system.  
  \#  
  \# Parameters:  
  \# - **username[in]**: cloud username  
  \# - **password[in]**: cloud password  
  \#  
  \#  **return**: success_code on success, failure_code on failure  
  ```python
  singstorage.connect(username, password)
  ```
 
- \# **(Required)** For writing data string to a distributed storage system.  
  \#  
  \# Parameters:  
  \# - **path[in]**: a logical path used for identifying the place where data has to written to (PROTOCOL:PATH_TO_DATA)  
  \# - **data[in]**: a Python string which stores the data to be written   
  \#    
  \#  **throw exception**: an exception is thrown if the operation is unsuccessful  
  ```python
  singstorage.write_data(path, data)
  ```
  
  
- \# **(Required)** For reading/accesing data from a distributed storage system.  
  \#  
  \# Parameters:  
  \# - **path[in]**: a logical path used for identifying the place where data is read from (PROTOCOL:PATH_TO_DATA)  
  \#   
  \#  **return**: a Python string in the provided data encoding (see above)  
  \#  **throw exception**: an exception is thrown if the operation is unsuccessful  
  ```python
  singstorage.read_data(path)
  ``` 
  
  - \# **(Required)** Close the communication session and release resources.  
  \#  
  \# Parameters:   
  ```python
  singstorage.close()
  ```
 
