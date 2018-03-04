# The file contains the user's context (its properties and io ops).
# The class given in the file encamsulates the state that the API has
# to maintain per active user.


# Python std modules
import sys



# Package modules
import singstorage.singexcept as errors




class UserProperties(object):
    """Class is a container for data processing properties.
       Properties are metadata for the stored data. For example,
       encoding, data compression, and so on.
    """

    __DATA_PROPERTIES__ = {"encoding" : {"utf-8":True}}
 

    def __init__(self):
        self._vals = {"encoding" : "utf-8"}



    def _check_key(self, prop_key):
        """Check if the property exists. If does not exist, throw
           an exception.
        """
        
        res = UserProperties.__DATA_PROPERTIES__.get(prop_key, None)
        if not res: # no such propery
            raise errors.PropertyException(
                        "Property \"{0}\" does not exist".format(key))

        return True



    def _check_val(self, prop_key, prop_val):
        """Check if the property can be set to the pro_val"""
        # get property options
        options = UserProperties.__DATA_PROPERTIES__.get(prop_key)
        if not options.get(prop_val, False): # cannot set to the value
            raise errors.PropertyException(
                "Property \"{0}\" cannot be set to \"{1}\".\nAvailable values: {2}".format(prop_key, prop_val, options))

        return True
            
    
    def get_property(self, prop_key):
        """Get the current property value.
           
           Args: 
             prop_key : property key
          
           return: key value
           throw : exception if no property found  
        """
        res = self._check_key(prop_key) # check if the key is fine


        # return the value of the given property
        return self._props[pop_key] 



    def set_properties(self, **kwargs):
        """ Set data properties
        """
        # Python2.7 
        if sys.version_info[0] == 2:
            for prop_key, prop_val in kwargs.iteritems():
                self._check_key(prop_key)
                self._check_value(prop_val)
                # can set the property
                self._props[prop_key] = prop_val 

        # Python3
        elif sys.version_info[0] == 3: 
            for prop_key, prop_val in kwargs.items():
                self._check_key(prop_key)
                self._check_val(prop_val)
                self._props[prop_key] = prop_val
                # can set the property

        else: # run time error as the system has no python version
            raise RuntimeError("Python Interpreter has no version.") 

                
             
        


class UserCtx(object):
    """ Stores user context for smooth communication between the 
        Python package and the system serivce that serves data
        requests to the sorage cluster. 
    """
