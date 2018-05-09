# -*- coding: utf-8 -*-

from __future__ import absolute_import
from __future__ import unicode_literals
from __future__ import division
from __future__ import print_function



from singstorage import PYTHON_MAJOR_VERSION, PYTHON_MINOR_VERSION


def encode_to_bytes(data_str):
	"""
		This function is used to enode the given string into
		bytes so that the returned array could be used for inteprocess
		communication.
	""" 

	# Python2.7
	if PYTHON_MAJOR_VERSION == 2 and PYTHON_MINOR_VERSION == 7:
		return data_str.encode("utf-8") # Python2 shall use unicode

	# Python3
	elif PYTHON_MAJOR_VERSION == 3: # Python3 uses unicode strings
		return data_str.encode("utf-8") 

	else:
		raise RuntimeError("Unrecognized Python interpreter.")
    

def decode_to_string(data_bytes):
	"""
		Decode a byte string to a Python string
	"""

	# Python2.7
	if PYTHON_MAJOR_VERSION == 2 and PYTHON_MINOR_VERSION == 7:
		return data_bytes
	
	# Python3
	elif PYTHON_MAJOR_VERSION == 3: # Pyton3 uses unicde strings
		return data_bytes.decode("utf-8")
	
	else:
		raise RuntimeError("Unrecognized Python interpreter.")



