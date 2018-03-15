# The file contains the IO operations between the Python APIs and the
# system service which serves API data storage requests.


# Python std lib
import logging

# singstorage modules
import singstorage.singexcept        as sing_errs
import singstorage.rados 	         as sing_rados
import singstorage.utils.loc_logging as sing_log

################ LOGGING ##################
# create a logger for this module
logger = logging.getLogger(__name__)

# create console handler and set level to debug
ch     = logging.StreamHandler()
ch.setLevel(sing_log.PACK_LOG_LEVEL) # set to only handle some logs

# create a custom formatter
formatter = logging.Formatter("[%(levelname)8s] - %(name)s:%(lineno)s ---- %(message)10s")

# add formatter to ch
ch.setFormatter(formatter)

# add handler to logger
logger.addHandler(ch)
logger.propagate=False

####### LOGGING ENDS HERE #############


####### Define some module exceptions #######
from singstorage.singexcept import StoreOpError as StorageError 


######### Exceptions end here  ################


def connect_to_cluster(user):
	if not user:
		raise sing_errs.AuthError()

	"""
		Connect to the sing service and raise an exception if
		any errors occurred, otherwise return sucess status
	"""
	
	return user.connect_to_service()



def write_data_sync(user, path, data):
	# need to convert 'data' to bytes
	rados_obj = sing_rados.RadosObject(user, path, data)

	# this method may throw an exception.
	user.write_raw_data(rados_obj)
    

def read_data_sync(user, path):
	rados_obj = sing_rados.RadosObject(user, path)

	# this may throw an exception
	user.read_raw_data(rados_obj)

	return rados_obj.get_raw_data() # return an instance of the 
									# Python bytearray


def delete_data_sync(user, path):
	rados_obj = sing_rados.RadosObject(user, path)

	# this may throw an exception
	user.delete_data(rados_obj)


def close_conn(user):
	# close the user
	if user:
		user.close()
