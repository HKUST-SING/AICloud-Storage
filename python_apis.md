### Python Distributed Storage APIs 

##### Python Remote Data Package (sing_data)
The Python package contains python interfaces for reading/writing data from a distributed storage system.
The very first design only provides a few basic functions for the users. It can be extended in the future.


- \# **(Optional)** Function can be used for changing the raw data storage proeprties.  
  \# A map is taken as an input to update current settings. 
  \#  
  \# Examples: encoding='UTF-8', compression=None  
  ```python
  sing_data.set_property(**kwards)
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
  sing_data.connect(username, password)
  ```
 
- \# **(Required)** For writing data string to a distributed storage system.  
  \#  
  \# Parameters:  
  \# - **path[in]**: a logical path used for identifying the place where data has to written to (PROTOCOL:PATH_TO_DATA)  
  \# - **data[in]**: a Python string which stores the data to be written   
  \#    
  \#  **throws exception**: an exception is thrown if the operation is unsuccessful  
  ```python
  sing_data.write_data(path, data)
  ```
  
  
- \# **(Required)** For reading/accesing data from a distributed storage system.  
  \#  
  \# Parameters:  
  \# - **path[in]**: a logical path used for identifying the place where data is read from (PROTOCOL:PATH_TO_DATA)  
  \#   
  \#  **return**: a Python string in the provided data encoding (see above)  
  \#  **throws exception**: an exception is thrown if the operation is unsuccessful  
  ```python
  sing_data.read_data(path)
