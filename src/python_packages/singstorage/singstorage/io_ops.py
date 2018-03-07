# The file contains the IO operations between the Python APIs and the
# system service which serves API data storage requests.

import singstorage.singexcept as sing_errs
import singstorage.rados 	  as sing_rados

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
	rados_obj = sing_rados.Rados(user, path, data)

	# this method may throw an exception.
	user.write_raw_data(rados_obj)
    

def read_data_sync(user, path):
	rados_obj = sing_rados.Rados(user, path)

	# this may throw an exception
	user.read_raw_data(rados_obj)

	return rados_obj.decode()


def close_conn(user):
	# close the user
	if user
		user.close()
