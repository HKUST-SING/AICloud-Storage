from __future__ import print_function

# import the singstorage module
from  context import singstorage 

import unittest


class TestSimpleIO(unittest.TestCase):

	def setUp(self):
		# create an instance of usercontext
		self.user = singstorage.context.UserContext("username", "password") 
		
		# try to connect to it
		self.user.connect_to_service()
		self.assertEqual(self.user.is_connected, True,
						 "Cannot connect to the service")


	def tearDown(self):
		if self.user:
			self.user.close()
			self.user = None

	def test_read_one(self):
		
		rados_obj = sinstoage.sing_rados.RadosObject(self.user, 
										 "blabla", bytearray(b"some_data"))
		with self.assertRaises(singstorage.StorageError):
			res = self.user.read_raw_data(rados_obj)

		

	def test_write_one(self):
		rados_obj = sinstoage.sing_rados.RadosObject(self.user, 
							"blabla", bytearray(b"some_data"))		
		with self.assertRaises(singstorage.StorageError):
			self.user.write_raw_data(rados_obj)

	
	def test_write_two(self):
		rados_obj = sinstoage.sing_rados.RadosObject(self.user, 
							"S3:PATH_VALUE", bytearray(b"some_data"))

		self.user.write_raw_data_(rados_obj)



	def test_read_two(self):
		rados_obj = sinstoage.sing_rados.RadosObject(self.user, 
						"S3:PATH_VALUE")

		self.user.read_raw_data(rados_obj)

		self.assertIsInstnace(rados_obj.get_raw_data(), bytearray,
							  "Rados object raw data is not a byte array")

		self.assertEqual(len(b"some_data"), len(rados_obj.get_raw_data()),
						"Returned byte array is not of the same size")



def run_test():
	"""
		For running a test suite.
	"""
	suite = unittest.TestSuite()
	suite.addTest(TestSimpleIO("test_read_one"))
	suite.addTest(TestSimpleIO("test_write_one"))
	suite.addTest(TestSimpleIO("test_write_two"))
	suite.addTest(TestSimpleIO("test_read_two"))
	runner = unittest.TextTestRunner()

	print(runner.run(suite))

if __name__ == '__main__':
	run_test()
