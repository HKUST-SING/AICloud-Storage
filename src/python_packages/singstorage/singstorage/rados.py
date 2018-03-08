# This file contains an abstraction and implementation of a RADOS 
# Gateway Objects for the Python side. Each time the user writes/reads
# an object, this class encapsulates the operation and data. The file
# is useful since it provides an encapsulation for very large files.


# Dependency packages


# Python std lib


# singstorage packages
import singstorage.usercontext   as sing_ctx
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




class RadosObject(object):
	"""
		RADOS Gateway-like object for a Python environment. Each user's 
		read/write operation creates a new instance of this class and
		the context of the user's data is stored by the instance.
	"""
	def __init__(self, user, data_path, data=bytearray(b""),
										props=None):
		self._user  =  user
		self._props =  props
		self._path  =  data_path
		self._data  =  data        # bytes of data    
		self._len   =  len(data)   # length of data  (in bytes)


	def get_bytes(self, start_idx=0, end_idx=-1):
		"""
			Get byte values in the range [start_idx, end_idx).
		"""
		if start_idx < 0 or end_idx >= self._len\
		   or start_idx >= self._len or end_idx < -1:
			raise IndexError("Provided indeces are out of range")

		if end_idx == -1:
			return self._data[start_idx::1]

		elif startd_idx > end_idx:
			raise IndexError("Wrong relationship between the lower and upper limits")

		else:
			return self._data[start_idx:end_idx:1]


	def get_raw_data(self):
		"""
			Return reference to the raw bytes.
		"""
		return self._data


	def extend_data(self, bin_data):
		"""
			For reading, append a chunk of binary data to 
			the data array.
		"""
		self._data.extend(bin_data)
		self._len += len(bin_data)


	def get_len(self):
		"""
			Get the length of current byte array in bytes
		"""
		return self._len 


	def get_data_path(self):
		"""
			Get the path entered by the user to access Ceph
			cluster.
		"""
		return self._path


	def get_rados_properties(self):
		"""
			Return a reference to the storage properties
			which define how the object is stored.
		"""
		return self._props


	def decode(self, encoding="utf-8"):
		"""
			This method is only if the raw data has to
			be converted into a Python string.
		"""
		self._data.decode(encoding=encoding)		
