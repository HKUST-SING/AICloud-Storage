# This file contains an abstraction and implementation of a RADOS 
# Gateway Objects for the Python side. Each time the user writes/reads
# an object, this class encapsulates the operation and data. The file
# is useful since it provides an encapsulation for very large files.


# Dependency packages


# Python std lib


# singstorage packages
from singstorage import PYTHON_MAJOR_VERSION






class RadosObj(object):
	"""
		RADOS Gateway-like object for a Python environment. Each user's 
		read/write operation creates a new instance of this class and
		the context of the user's data is stored by the instance.
	"""
	def __init__(self, user, data_path, data=""):
		self._user = user
		self._path = data_path
		self._data = bytearray(data, 
					 encoding="utf-8") # raw binary data (bytes)
		self._len  = len(data)         # length of data  (in bytes)


	def get_bytes(self, start_idx=0, end_idx=-1):
		"""
			Get byte values in the range [start_idx, end_idx).
		"""
		if start_idx < 0 or end_idx >= self._len\
		   or start_idx >= self._len or end_idx < -1:
			raise IndexError("Provided indeces are out of range")

		if end_idx == -1:
			return self._data(start_idx::1)

		elif startd_idx > end_idx:
			raise IndexError("Wrong relationship between the lower and upper limits")

		else:
			return self._data(start_idx:end_idx:1)


	def get_raw_data(self):
		"""
			Return reference to the raw bytes.
		"""
		return self._data


	def extend_data(self, bin_data):
		self._data.extend(bin_data)
		self._len += len(bin_data)


	def get_len(self):
		return self._len 

	def get_data_path(self):
		return self._path


	def decode(self, encoding="utf-8"):
		self._data.decode(encoding=encoding)		
