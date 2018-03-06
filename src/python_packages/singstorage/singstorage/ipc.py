from __future__ import print_function


#import SocketServer
import socket
import struct
import ctypes

PORT = 4997

RWDATA = 1
ERRORMSG = 2

FORMAT_RWDATA = '=BBIQ'
FORMAT_ERRORMSG = '=BBH'

MESSAGESIZE = 16

class Message(object):
	# the msg_length will be computed after all content are filled
	def set_generic(self,msg_type):
		self.msg_type = msg_type

	def compute_msg_length(self):
		self.msg_length = ctypes.sizeof(ctypes.c_uint8()) + \
		ctypes.sizeof(ctypes.c_uint8())

		if self.msg_type == RWDATA:
			self.msg_length += ctypes.sizeof(ctypes.c_uint32()) + \
			ctypes.sizeof(ctypes.c_uint64())
		elif self.msg_type == ERRORMSG:
			self.msg_length += ctypes.sizeof(ctypes.c_uint16())

	def set_data(self,data_length,starting_address):
		if self.msg_type != RWDATA:
			# this message is not for read/write data, so it shouldn't
			# be filled with data attributes
			# TODO: throw an error or exception
			pass
		else:
			self.data_length = data_length
			self.starting_address = starting_address
			self.compute_msg_length()

	def set_error_type(self,error_type):
		if self.msg_type != ERRORMSG:
			# this message is not for error message, so it shouldn't
			# be filled with error type
			# TODO: throw an error or exception
			pass
		else:
			self.error_type = error_type
			self.compute_msg_length()
'''
class Server(SocketServer.ThreadingUnixStreamServer):
	def __init__(self,server_address):
		self.address_family = socket.AF_UNIX

		class IPCHandler(SocketServer.BaseRequestHandler):
			def handle(self):
				print 'start handling'
			
				msg = self.request.recv(2*ctypes.sizeof(ctypes.c_uint8()))
				print 'recv'+str(len(msg))
				msg_type,msg_length = struct.unpack('BB',msg)
				msg = self.request.recv(msg_length-2*ctypes.sizeof(ctypes.c_uint8()))
				if msg_type == RWDATA:
					data_length, starting_address \
					= struct.unpack(FORMAT_RWDATA[2:],msg)
					print msg_type,msg_length,data_length,starting_address
				elif msg_type == ERRORMSG:
					error_type, \
					= struct.unpack(FORMAT_ERRORMSG[2:],msg)
					print msg_type,msg_length,error_type
				else:
					# the message type is undefined
					pass
				# handle receive data


		#SocketServer.TCPServer(self,server_address,IPCHandler)
		SocketServer.TCPServer.__init__(self,server_address,IPCHandler)
'''
class Client(object):
	def __init__(self,server_address):
		self.addr = server_address
		self.sock = socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)

	def connect(self):
		self.sock.connect(self.addr)

	def close(self):
		self.sock.close()

	def send(self,message):
		if message.msg_type == RWDATA:
			msg = struct.pack\
			(FORMAT_RWDATA,message.msg_type,message.msg_length,\
				message.data_length,message.starting_address)
		elif message.msg_type == ERRORMSG:
			msg = struct.pack\
			(FORMAT_ERRORMSG,message.msg_type,message.msg_length,message.error_type)
		else:
			# the message type is undefined
			pass
		self.sock.sendall(msg)
		read_len = len(msg)
		read_bytes = 0
		final_data = []
		while read_bytes < read_len:
			data = self.sock.recv((read_len - read_bytes), 0)
			if not data:
				print ("Erorr reading data back")
				break
			read_bytes += len(data)
			final_data.append(data)


        # received message
		recv_msg = "".join(final_data)
		msg_type,msg_length = struct.unpack('=BB',recv_msg[0:2])
		if msg_type == RWDATA:
			data_length, starting_address\
                = struct.unpack('='+FORMAT_RWDATA[3:],recv_msg[2:])

			print (msg_type,msg_length,data_length,starting_address)

		elif msg_type == ERRORMSG:
			error_type,\
			    = struct.unpack('='+FORMAT_ERRORMSG[3:],recv_msg[2:])
			print (msg_type,msg_length,error_type)
		else:
			# the message type is undefined
			pass
			# handle receive data
            
		print ('client success')
		
		
if __name__ == '__main__':
	'''
	import threading
	s = Server('localhost:'+str(PORT))
	print '1'
	t1 = threading.Thread(target = s.serve_forever)
	t1.daemon = True
	t1.start()
	print '2'
	'''
	c = Client('echo_socket')
	c.connect()
	
	m = Message()
	m.set_generic(ERRORMSG)
	m.set_error_type(8)
	m.compute_msg_length()

	c.send(m)

	n = Message()
	n.set_generic(RWDATA)
	n.set_data(16,3369)
	n.compute_msg_length()

	c.send(n)
	c.close()
	'''
	import time
	time.sleep(10)
	s.shutdown()
	s.server_close()
	'''	
