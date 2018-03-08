# The file contains the IO operations between the Python APIs and the
# system service which serves API data storage requests.

import singstorage.singexcept    as sing_errs
import singstorage.rados 	     as sing_rados
import singstorage.utils.logging as sing_log

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


####### LOGGING ENDS HERE #############



def connect_to_cluster(user):
	if not user:
		raise RuntimeError("User has not been initialized")

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


def close_conn(user):
	# close the user
	if user:
		user.close()
