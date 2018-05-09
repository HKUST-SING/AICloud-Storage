# -*- coding: utf-8 -*-

from __future__ import absolute_import
from __future__ import unicode_literals
from __future__ import division
from __future__ import print_function


# Python std lib
import sys

# check if valid Python versions
PYTHON_MAJOR_VERSION = sys.version_info[0]
PYTHON_MINOR_VERSION = sys.version_info[1]

if PYTHON_MAJOR_VERSION == 2 and PYTHON_MINOR_VERSION != 7:
	sys.exit("Only Python2 with minor 7 is supported")

if PYTHON_MAJOR_VERSION != 2 and PYTHON_MAJOR_VERSION != 3:
	sys.exit("Unsupported Python version: {0}".format(PYTHON_MAJOR_VERSION))
	

# manage user level libraries so that it would
# easy for the user of the package to directly
# import user-level packages




# import singsotrage packages
import singstorage.utils.loc_logging # enable logging
import singstorage.singexcept            as errors
import singstorage.internal.io_ops       as ops
import singstorage.internal.messages     as messages
import singstorage.internal.newcontext   as context
import singstorage.internal.rados        as rados




################### Define values available for user ###################
from   singstorage.singexcept import StoreOpError as IOException
from   singstorage.singexcept import SUCCESS      as SUCCESS
from   singstorage.singexcept import AUTH_USER    as AUTH_USER
from   singstorage.singexcept import AUTH_PASSWD  as AUTH_PASSWD



################### Public User Definitions End Here ###################

# per user session context
cloud_user = None
gl_properties = context.StorageProperties()


def connect(username, password):

	global cloud_user
	
	# initialize a user and try to connect to the storage service
	if not cloud_user:
		cloud_user = context.UserContext(username, password)
		cloud_user.set_properties(gl_properties)

	return ops.connect_to_cluster(cloud_user)


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

	ops.close_conn(cloud_user)
	# reset the close user to None
	cloud_user = None



def get_error_message(err_num):
	
	err_msg = errors.ERROR_MSGS.get(err_num, None)

	if err_msg:
		return err_msg
	else:
		return "\n'{0}' error code does not exist.\n".format(err_num)


def set_properties(**kwargs):

	"""
		Function updates the user's internal state that
		considers data properties.
	"""   
	gl_properties.set_properties(**kwargs)
