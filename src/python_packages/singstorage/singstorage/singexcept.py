# The file contains custom exceptions for authentication 
# and IO opearations.



# Since the file accommodates all the user exceptions, it has been
# decided that the file should also accommodate all the error codes
# and error strings.

SUCCESS          =   0   # successful authentication
AUTH_USER        =   1   # no such user exists
AUTH_PASSWD      =   2   # wrong password
# ---To be continue
INTERNAL_ERROR   =   3   # internal IO error


# Dictionary mapping erorr codes to error messages
ERROR_MSGS = {
	AUTH_SUCCESS    :  "\nSuccessfully connected to the cluster.\n",
	AUTH_USER       :  "\nWrong username.\n",
	AUTH_PASSWD     :  "\nWrong password for input user.\n",
	INTERNAL_ERROR  :  "\nInternal system error.\n"
			 }



class PropertyException(Exception):
	"""Thrown when the user provides an invalid property or its value."""
	def __init__(self, message, prop, value):
		super(PropertyException, self).__init__()
		self._msg   = message


	def __str__(self):
		return self._msg


class SingIOError(Exception):
	"""Base class for exceptions in this module."""

	def __init__(self, message, err_code):
		super(IOError, self).__init__(message)
		self.message  = message
		self.err_code = err_code
   

class QuotaError(SingIOError):
	"""
		This exception is thrown if the user has exceeded his/her 
		storage quota.
	"""

	def __init__(self, message, tried_write):
		super(QuotaError, self).__init__(message)
		self._failed_write = tried_write


	def __str__(self):
		return "{0}\nCannot write: {1} bytes.\n".format(
			super.message, self._failed_write)



class ProtError(SingIOError): 
	"""
		This exception is thrown if there are any problems 
		with the user's selected protocol.
	"""
	def __init__(self, message, protocol):
		super(ProtError, self).__init__(message)
		self._prot = protocol


	def __str__(self):
		return "{0}\nProtocol value: {1}\n".format(
			super.message, self._prot)



class PathError(SingIOError):
	"""
		This exception is thrown is the given path/URL to data
		has problems.
	"""

	def __init__(self, message, given_path):
		super(PathError, self).__init__(message)
		self._dpath = given_path


	def __str__(self):
		return "{0}\nData path: \"{1}\"\n".format(
			super.message, self._dpath)

    
class AuthError(SingIOError):
	"""
		This exception is thrown if IO operations are used
		before connecting to the data cluster.
	"""
	def __init__(self, message):
		super(AuthError, self).__init__(message)


	def __str__(self):
		return super.message
