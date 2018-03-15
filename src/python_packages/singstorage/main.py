import singstorage


user = None

try:
	user = singstorage.context.UserContext("username", "password")
	if not user:
		raise IOError("Out of Memory")

	user.connect_to_service()

	rados_obj = singstorage.rados.RadosObject(user, "some_bla")

	user.read_raw_data(rados_obj)

except Exception as exp:
	print exp

finally:
	if user:
		user.close()

