# The file contains the IO operations between the Python APIs and the
# system service which serves API data storage requests.

import singstorage.singexcept as singerrs


def connect_to_cluster(user):
	if not user:
		raise RuntimeError("User has not been initialized")

	"""
		Connect to the sing service and raise an exception if
		any errors occurred, otherwise return sucess status
	"""
	try:
		user.connect_to_service()

	except SingIOException as exp:
		return exp.err_code

	except: # some generic error code
		return singerrs.INTERNAL_ERROR

	# successful connection establishment
	return singerrs.AUTH_SUCCESS


def write_data_sync(user, path, data):
	pass
    

def read_data_sync(user, path):
	pass
	return ""


def close_conn(user):
	# close the user
	user.close()
