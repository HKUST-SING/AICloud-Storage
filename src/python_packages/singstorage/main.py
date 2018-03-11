from __future__ import print_function

import singstorage

try:
	singstorage.connect("John", "password")

	singstorage.read_data("S3:path_to_data")
	singstorage.write_data("S3:path_to_data", bytearray(b"ONE MESSAGE TO WRITE"))

	singstorage.close()


except Exception as exp:
	print(exp)
