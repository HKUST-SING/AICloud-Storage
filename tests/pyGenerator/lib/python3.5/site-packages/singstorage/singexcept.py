# -*- coding: utf-8 -*-

from __future__ import absolute_import
from __future__ import unicode_literals
from __future__ import division
from __future__ import print_function

# The file contains custom exceptions for authentication 
# and IO opearations.



# Since the file accommodates all the user exceptions, it has been
# decided that the file should also accommodate all the error codes
# and error strings.

# Authentication errors (also return codes)
SUCCESS          =   0  # successful authentication
AUTH_USER        =   1  # no such user exists
AUTH_PASSWD      =   2  # wrong password



# Generic error (for out of memory, socket errors, read errors, etc)
INTERNAL_ERROR   =   255   # internal system error


# Dictionary mapping erorr codes to error messages
ERROR_MSGS = {
	SUCCESS         :  "\nSuccessfully connected to the cluster.\n",
	AUTH_USER       :  "\nWrong username.\n",
	AUTH_PASSWD     :  "\nWrong password for the given username.\n",
	INTERNAL_ERROR  :  "\nInternal system error. Report to the system administrator.\n"
			 }



# SYSTEM INTERNAL_PROBLEMS/ERRORS
INT_ERR_MEMORY   =  1 # memory problems
INT_ERR_IPC	     =  2 # interprocess-communication (control) problems
INT_ERR_READ     =  3 # unavailability to read data internally
INT_ERR_WRITE    =  4 # unavailability to write data internally
INT_ERR_DATA     =  5 # user data corruption problems
INT_ERR_PROT	 =  6 # internal communication protocol error
INT_ERR_UNKNOWN  =  254 # unkown system error

# Dictionary for categorizing internal errors.
__INTERNAL_ERRORS__ = {
	INT_ERR_MEMORY   : "System Memory Related Error.",
	INT_ERR_IPC	     : "System Inter-process Communication Error.",
	INT_ERR_READ	 : "System Internal Data Read Error.",
	INT_ERR_WRITE	 : "System Internal Data Write Error.",
	INT_ERR_DATA	 : "User Data Corruption Error.",
	INT_ERR_PROT	 : "Internal Communication Protocol Error.",
	INT_ERR_UNKNOWN  : "Unknown System Error."
					  }



class StoreOpError(Exception):
	"""Base class for exceptions in this module."""

	def __init__(self, message):
		super(StoreOpError, self).__init__(message)
		


	def __str__(self):
		tmp_msg = super(StoreOpError, self).__str__() 
		
		return "".join([tmp_msg, "\n\n"])



class PropertyException(StoreOpError):
	"""Thrown when the user provides an invalid property or its value."""
	def __init__(self, prop, value="", av_ops=None):
		
		if not value:
			super(PropertyException, self).__init__("Storage property '{0}' does not exist.".format(prop))
		else:
			super(PropertyException, self).__init__("Storage property '{0}' cannot be set to value '{1}'.\nPossible options: {2}.".format(prop, value, av_ops))
		

class QuotaError(StoreOpError):
	"""
		This exception is thrown if the user has exceeded his/her 
		storage quota.
	"""

	def __init__(self, tried_write, can_write):
		super(QuotaError, self).__init__("You cannot write {0} bytes of data as you are only allowed to store {1} bytes more of data.".format(tried_write, can_write))
	


class ProtError(StoreOpError): 
	"""
		This exception is thrown if there are any problems 
		with the user's selected protocol.
	"""
	def __init__(self, protocol):
		super(ProtError, self).__init__("Your selected data transfer protocol, '{0}', is not supported by the storage system.".format(protocol))



class PathError(StoreOpError):
	"""
		This exception is thrown is the given path/URL to data
		has problems (NOT FOUND or user is not allowed to access it).
	"""

	def __init__(self, given_path, access_rights=False):
		if access_rights:
			super(PathError, self).__init__("You have no access rights to access data at '{0}'.".format(given_path))
		else:
			super(PathError, self).__init__("Your given data address, '{0}', cannot be found.".format(given_path))
		

    
class AuthError(StoreOpError):
	"""
		This exception is thrown if IO operations are used
		before connecting to the data cluster.
	"""
	def __init__(self, err_msg=""):
		if err_msg:
			super(AuthError, self).__init__(err_msg)
		else:
			super(AuthError, self).__init__("You are not connected to the storage service.\nPlease connect to the service before performing\nany data I/O operations.\n")


class DataError(StoreOpError):
	"""
		This exception is thrown if the passed write
		data is not a string of bytes.
	"""
	def __init__(self, message):
		super(DataError, self).__init__(message)



class InternalError(StoreOpError):
	"""
		This exception is thrown if the system experiences 
		problems not related to the user. For example, 
		if the system runs out of memory, cannot connect
		read/write data internally, and etc.
	"""

	def __init__(self, error_code):
		super(InternalError, self).__init__("Internal system error:'{0}'".format(__INTERNAL_ERRORS__[error_code]))
