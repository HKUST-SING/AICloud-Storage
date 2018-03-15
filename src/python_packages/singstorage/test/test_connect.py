from __future__ import print_function

# import the singstorage module
from  context import singstorage 

import unittest


class TestConnect(unittest.TestCase):

	def setUp(self):
		# create an instance of usercontext
		self.user = singstorage.context.UserContext("username", "password") 


	def tearDown(self):
		if self.user:
			self.user.close()
			self.user = None

	def test_connect(self):
		self.user.connect_to_service()
		self.assertEqual(self.user.is_connected(), True,
						"Service is not connected")



def run_test():
	"""
		For running a test suite.
	"""
	suite = unittest.TestSuite()
	suite.addTest(TestConnect("test_connect"))
	runner = unittest.TextTestRunner()

	print(runner.run(suite))

if __name__ == '__main__':
	run_test()
