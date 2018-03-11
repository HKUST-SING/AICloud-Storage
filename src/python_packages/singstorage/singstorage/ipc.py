# Modules which contains the interprocess communication components.
# This module should contain most of the logic and implementation
# needed for the user.


# Dependency packages



# Python std lib
import socket
import time
import errno
import logging


# Package modules
import singstorage.singexcept        as sing_errs
from   singstorage.messages          import InterMessage  
import singstorage.messages	     as sing_msgs
import singstorage.utils.loc_logging as sing_log



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

# Inter-process control types
CONTROL_EMPTY  = 0
CONTROL_SOCKET = 1
CONTROL_PIPE   = 2
CONTROL_MEMORY = 3



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

		# get logger
		tmp_logger = logging.getLogger(__name__)

		if not self._sock or not self._rem_addr:
			# log the case
			tmp_logger.error("Control socket or remote addres is None.")

			raise sing_errs.InternalError(sing_errs.INT_ERR_MEMORY)


		# try to connect to the service
		res = self._sock.connect_ex(self._rem_addr)

		if res != 0: # try a few more times
		
			tmp_logger.warn("Control Socket did not connect at the first attempt. res_code: {0}".format(res))

			# the only error which is handled, ECONNREFUSED
			count = 3 # try a few times if ECONNREFUSED
		
			while count > 0 and res == errno.ECONNREFUSED:
				time.sleep(1.0) # give some time to the service	
				count -= 1      # update counter
				res  = self._sock.connect_ex(self._rem_addr)


			if res != 0: # not a success
				tmp_logger.error("Cannot connect to the service after a few attempts.")
				raise sing_errs.InternalError(sing_errs.INT_ERR_IPC)

		# connection has successfully connected
		# authenticate the user
		tmp_logger.info("Socket has connected to the storage service.")

		self.send_request(sing_msgs.MSG_AUTH, username=username,
						  passwd=password)

		# wait for a confirmation from the service
		
		

	def send_request(self, req_type, **kwargs):
		"""
			Send a request to the service.
		"""

		tmp_logger = logging.getLogger(__name__)
		tmp_logger.info("send_request: {0}".format(req_type))
		
		# create a message
		msg = InterMessage.create_message(req_type, **kwargs)
		
		if not msg:
			tmp_logger.error("Cannot create an IPC message in send_request")
			
			raise sing_errs.InternalError(sing_errs.INT_ERR_IPC)

		# send the message
		bin_data = msg.encode_msg() # encode the message into 
								 	# a binary string


		# try to write the message 
		tmp_logger.info("Encoded message is being sent to the service")
		self._sock.sendall(bin_data, 0)
		res_msg = self.recv_request(sing_msgs.MSG_STATUS)

		return res_msg.op_status # return the status



	def recv_request(self, req_type, **kwargs):
		"""
			Receive a request from the service.
		"""

		tmp_logger = logging.getLogger(__name__)
		tmp_logger.info("recv_request: ".format(req_type))

		msg = InterMessage.create_message(req_type, **kwargs)
		

		if not msg:
			tmp_logger.error("recv_request: msg is None")
			raise sing_errs.InternalError(sing_errs.INT_ERR_IPC)

		# read the header of the received message
		header = self._sock.recv(msg.get_header_size(), 0)
               	

		if len(header) != msg.get_header_size() or\
			msg.msg_type != req_type:

			tmp_logger.error("wrong msg: exp: {0}, received: {1}".format(req_type, msg.msg_type))
			raise sing_errs.InternalError(sing_errs.INT_ERR_PROT)	

		# decode the header
		msg.decode_header(header)
		left_to_read = msg.msg_length - msg.get_header_size()
		raw_data = [] # store a list of binary strings	

		while left_to_read > 0: # read until data is read or 
								# an error occurs

			read_data = self._sock.recv(left_to_read, 0)
			
			if not read_data:
				# Error occured
				tmp_logger.error("socket.recv returns None")
				raise sing_errs.InternalError(sing_errs.INT_ERR_UNKNOWN)

			raw_data.append(read_data)
			left_to_read -= len(read_data)


		# message has successfully been read
		
		msg.decode_msg(b"".join(raw_data)) # decode the raw data
										   # to a message



		return msg # return the read message


	def close_conn(self):
		"""
			Close the UNIX socket.
		"""

		if not super._connected:
			return # the IPC is closed


		tmp_logger = logging.getLogger(__name__)
		tmp_logger.info("close_conn")

		# send a notification to release 
		# the resources

		self.send_request(sing_msgs.MSG_STATUS,
					      status_type=sing_msgs.STAT_CLOSE)



		# wait for response
		res = self.recv_request(sing_msgs.MSG_STATUS)

		if res.op_status != sing_msgs.STAT_SUCCESS:
			# log the response
			tmp_logger.warn("Received status to close is not SUCCESS. status: {0}".format(res.op_status))

			# check for ambiguous status
			if res.op_status == sing_msgs.STAT_AMBG:
				
				# send one more time
				tmp_logger.warn("received STAT_AMG")

				self.send_request(sing_msgs.MSG_STATUS,
								  status_type=sing_msg.STAT_CLOSE)

				res = self.recv_request(sing_msgs.MSG_STATUS)

				# log the response
			

		# close the socket and set the IPC as inactive
		self._sock.close()
		super._connected = False
	

