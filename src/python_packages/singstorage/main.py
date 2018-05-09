from __future__ import print_function


import sys

import singstorage


user = None

try:
	user = singstorage.context.UserContext("username", "password")
	if not user:
		raise IOError("Out of Memory")

	user.connect_to_service()

	rados_obj = singstorage.rados.WriteContext("some_random_Path",
												bytearray(b"X"*200*1024*1024))

	user.append_operation(rados_obj)

	if isinstance(rados_obj.get_result(), singstorage.errors.StoreOpError):
		print ("received an exception")
		raise rados_obj.get_result()

	else:
		print("Write Completed: {0}".format(rados_obj.get_result()))



except Exception as exp:
	print (exp)

finally:
	if user:
		user.close()
