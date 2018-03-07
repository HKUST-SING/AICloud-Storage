# Modules which contains the interprocess communication components.
# This module should contain most of the logic and implementation
# needed for the user.


# Dependency packages



# Python std lib:w
import socket
import time
import errno


# Package modules
import singstorage.singexcept as sing_errs
from   singstorage.messages import InterMessage  



CONTROL_SOCKET = 0
CONTROL_PIPE   = 1
CONTROL_MEMORY = 2



class ControlIPC(object):
	"""
		Generic interface for the control part of the 
		inter-process communication. Different implementation
		can be provided.
	"""

	subcls = {} # dictionary of IPC subclasses


	def __init__(self):
		self._connected = False # is an active ipc


	def init_ipc(self):
		"""
			Initialization method of the IPC methods. Call this
			method before the other methods get called.
		"""
		raise NotImplementedError("Method is not implemented.")



	def connect_to_service(self, username, password):
		"""
			Connect to the sing storage service
		"""
		raise NotImplementedError("Method is not implemented.")


	def close_conn(self):
		"""
			Close the connection to the sing service	
		"""
		raise NotImplementedError("Method is not implemented.")


	def is_connected(self):
		return self._connected
	


	def send_request(self, req_type, **kwargs):
		raise NotImplementedError("Method is not implemented.")


	def recv_request(self, req_type, **kwargs):
		raise NotImplementedError("Method is not implemented.")



	@classmethod
	def register_subclass(cls, ipc_type):
		"""
			Class decorator for subclasses to register with
			the super class.
		"""		
		def add_sub(subclass):
			cls.subcls[ipc_type] = subclass
			
			return subclass

		return add_sub # register all IPC subclasses


	@classmethod
	def create_control_ipc(cls, ipc_type, **kwargs):
		"""
			Create and initialize an ipc handle.
		"""
		IPCMethod = cls.subcls.get(ipc_type, None)

		if not IPCMethod:
			return None

		# create a control ipc handle
		return IPCMethod(**kwargs)



@ControlIPC.register_subclass(CONTROL_SOCKET)
class SocketIPC(ControlIPC):
	"""
		A control IPC implementation using UNIX domain sockets.
	"""		

	def __init__(self):
		super(SocketIPC, self).__init__()
		self._socket   = None
		self._rem_addr = None
	


	def init_ipc(self):
		# read the socket configuration file
		self._rem_addr = "/tmp/sing_ipc_socket"
		self._sock     = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)


	def connect_to_service(self, username, password):
		if not self._sock or not self._rem_addr:
			raise RuntimeError("Not Initialized Socket Strucutres.")


		# try to connect to the service
		res = self._sock.connect_ex(self._rem_addr)
		
		# the only error which is handled, ECONNREFUSED
		count = 3 # try a few times if ECONNREFUSED
		
		while count > 0 and res == errno.ECONNREFUSED:
			time.sleep(1.0) # give some time to the service	
			count -= 1      # update counter
			res  = self._sock.connect_ex(self._rem_addr)


		if res != 0: # not a success
			raise IOError("Cannot connect to the sing service")

		# connection has successfully connected
		# authenticate the user
		self.send_request(sing_msgs.MSG_AUTH, username=username,
						  passwd=password)
		
		

	def send_request(self, req_type, **kwargs):
		"""
			Send a request to the service.
		"""

		# create a message
		msg = InterMessage.create_message(req_type, **kwargs)
		
		if not msg:
			raise IOError("No such request type.")

		# send the message
		bin_data = msg.encode_msg() # encode the message into 
								 	# a binary string


		# try to write the message 
		self._sock.sendall(bin_data, 0)



	def recv_request(self, req_type, **kwargs):
		"""
			Receive a request from the service.
		"""
		msg = sing_msgs.create_message(req_type, **kwargs)
		
		if not msg:
			raise IOError("No such request type.")

		# read the header of the received message
		header = self._sock.recv(msg.get_header_size(), 0)
		

		if len(header) != msg.get_header_size() or\
			msg.msg_type != req_type:
			raise IOError("Error: reading header:")	

		# decode the header
		msg.decode_header(header)
		left_to_read = msg.msg_length - msg.get_header_size()
		raw_data = [] # store a list of binary strings	

		while left_to_read > 0: # read until data is read or 
								# an error occurs

			read_data = self._sock.recv(left_to_read, 0)
			
			if not read_data:
				# Error occured
				raise IOError("Error: reading message content.")

			raw_data.append(read_data)
			left_to_read -= len(read_data)


		# message has successfully been read
		
		msg.decode_msg(b"".join(raw_data)) # decode the raw data
										   # to a message


		return msg # return the read message


	def close_conn(self):
		self._sock.close()
		super._connected = False
	

