# -*- coding: utf-8 -*-

# The file contains the IO operations between the Python APIs and the
# system service which serves API data storage requests.

from __future__ import absolute_import
from __future__ import unicode_literals
from __future__ import division
from __future__ import print_function


# Python std lib
import logging

# singstorage modules
import singstorage.singexcept        as sing_errs
import singstorage.internal.rados 	 as sing_rados
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


def _check_write_params(data):
	"""
		Does checking of the passed parameters.
		If any failure, throws an appropriate 
		exception.
	"""

	if not data or len(data) == 0:
		raise sing_errs.DataError("There is no data to write.")

	# need to check if the passed data
	# supports the Python buffer protocol
	bufferProt = True

	try:
		memoryview(data)
	except:
		bufferProt = False # does not support


	if bufferProt is False:
		raise sing_errs.DataError("Your passed data does not support\nthe Python memory buffer protocol.")



def _check_generic_params(user, path):
	"""
		Does checking of the passed parameters.
		If any failure, throws an appropriate 
		exception.
		This one is a generic function for
		most of IO operation parameter checking.

	"""
	if not user or not user.is_connected():
		raise sing_errs.AuthError()

	if not path or len(path) == 0:
		raise sing_errs.PathError("", False)



def connect_to_cluster(user):
	if not user:
		return sing_errs.AUTH_USER

	if user.is_connected():
		return sing_errs.SUCCESS

	"""
		Connect to the sing service and raise an exception if
		any errors occurred, otherwise return sucess status
	"""
	
	return user.connect_to_service()



def write_data_sync(user, dpath, raw_data):

    # if anything is bad, throws an excpetion 
	_check_generic_params(user, dpath)
	_check_write_params(raw_data)


	# need to convert 'data' to bytes
	write_data_op = sing_rados.WriteContext(dpath, raw_data)

	# this method may throw an exception.
	user.append_operation(write_data_op)

	if isinstance(write_data_op.get_result(), StorageError):
		raise write_data_op.get_result()

	# write does nothing on success
    

def read_data_sync(user, dpath):
	
	# if anything is wrong with the parameters
	# throws an exception
	_check_generic_params(user, dpath)
	
	read_data_op = sing_rados.ReadContext(dpath)

	# this may throw an exception
	user.append_operation(read_data_op)

	if isinstance(read_data_op.get_result(), StorageError):
		raise read_data_op.get_result()

	else:
		# return a READ operation handler
		return read_data_op



def delete_data_sync(user, dpath):

	# throws an exception if anything
	# is wrong
	_check_generic_params(user, dpath)

	del_data_op = sing_rados.DeleteContext(dpath)

	# throw an exception if there is one
	user.append_operation(del_data_op)

	if isinstance(del_data_op.get_result(), StorageError):
		raise del_data_op.get_result()

	# delete does not do anything on success


def close_conn(user):
	# close the user
	if user:
		user.close()
