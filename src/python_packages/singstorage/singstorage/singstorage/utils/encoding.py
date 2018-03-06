import sys


def encode_to_bytes(data_str):
	"""
		This function is used to enode the given string into
		bytes so that the returned array could be used for inteprocess
		communication.
	""" 

	# get Pyhton interpreter version
	py_ver_major = sys.version_info[0] 
	py_ver_minor = sys.version_info[1]   

	# Python2.7
	if py_ver_major == 2 and py_ver_minor == 7:
		return data_str # Python2 strings are byte strings

	# Python3
	elif py_ver_major == 3: # Python3 uses unicode strings
		return data_str.encode("utf-8") 

	else:
		raise RuntimeError("Unrecognized Python interpreter.")
    




