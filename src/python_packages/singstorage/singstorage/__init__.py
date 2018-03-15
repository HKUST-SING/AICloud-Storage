# Python std lib
import sys

# check if valid Python versions
PYTHON_MAJOR_VERSION = sys.version_info[0]
PYTHON_MINOR_VERSION = sys.version_info[1]

if PYTHON_MAJOR_VERSION == 2 and PYTHON_MINOR_VERSION != 7:
	sys.exit("Only Python2 with minor 7 is supported")

if PYTHON_MAJOR_VERSION != 2 and PYTHON_MAJOR_VERSION != 3:
	sys.exit("Unsupported Python version: {0}".format(PYTHON_MAJOR_VERSION))
	




# import singsotrage packages
import singstorage.utils.loc_logging # enable logging
import singstorage.singexcept   as errors
import singstorage.io_ops       as ops
import singstorage.usercontext  as context
import singstorage.messages     as messages


# per user session context
cloud_user = None
gl_properties = context.StorageProperties()


def connect(username, password):

	global cloud_user
	if cloud_user and cloud_user.is_connected(): # already connected 
		raise errors.AuthError("User connected.")

	# initialize a user and try to connect to the storage service
	if not cloud_user:
		cloud_user = context.UserContext(username, password)
		cloud_user.set_properties(gl_properties)

	ops.connect_to_cluster(cloud_user)


def write_data(dpath, data):
	global cloud_user
	ops.write_data_sync(cloud_user, dpath, data)
      

def read_data(dpath):
	global cloud_user
	return ops.read_data_sync(cloud_user, dpath)


def delete_data(dpath):
	global cloud_user
	ops.delete_data_sync(cloud_user, dpath)


def close():
	global cloud_user

	if not cloud_user:
		return 

	ops.close_conn(cloud_user)
	# reset the close user to None
	cloud_user = None


def set_properties(**kwargs):

	"""
		Function updates the user's internal state that
		considers data properties.
	"""   
	gl_properties.set_properties(**kwargs)
