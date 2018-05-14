from __future__ import print_function


# This is client simulator.
# Simulator will produce requests by calling
# `singstorage` module.
# Requests workload satisfy workload file,
# which is created by `workloadGen`.

import singstorage

READ = 0
WRITE = 1

UNIT = 1 # Byte
LOG_FILE = "log.txt"
USERNAME = "SingTest"
PASSWORD = "SimplePassword"

# Get the workload list.
# Entry Format:
# [int,string(,int)]
def getWorkload(workload):
	workloadlist = []
	with open(workload,'r') as f:
		entry = f.readline()
		while entry:
			# Decode to string
			#bytes.decode(entry)

			# Entry begins from '#' indicating
			# it is a description
			if entry[0].isdigit():
				# Strip space and '\n'
				entry = entry.lstrip("\n \t")
				entry = entry.rstrip("\n \t")

				# Split entry
				entry = entry.split(',')

				# Translate value from string to int
				entry[0] = int(entry[0])
				# If this write request, resolve the third element
				if entry[0] == WRITE:
					entry[2] = int(entry[2])*UNIT

				# Append to workload list
				workloadlist.append(entry)

			entry = f.readline()

	# Return workload list
	return workloadlist


def process(username,passwd,requestList,logFile=LOG_FILE):
	with open(logFile,'a') as log:
		res_code = singstorage.connect(username, passwd)
		if res_code == singstorage.SUCCESS:
			print('Connected to the service.')
			run_ops = 0
			count   = 0
			while run_ops < len(requestList) and\
				run_ops < 2:

				try:
					if requestList[run_ops][0] == READ:
						r_handle = singstorage.read_data("/sing"+requestList[run_ops][1])
						for some_data in r_handle:
							pass
					elif requestList[run_ops][0] == WRITE:
						singstorage.write_data("/sing"+requestList[run_ops][1], bytearray(b"x"*requestList[run_ops][2]))
				except singstorage.IOException as exp:
					log.writelines("Request {0} fail. {1}\n".format(count,exp))

				else:
					log.writelines("Finish Request{0}\n".format(count))
				finally:
					run_ops += 1
					count += 1
					

		else:
			print(singstorage.get_error_message(res_code))


if __name__ == '__main__':
	wlist = getWorkload("file.txt")

	process(USERNAME, PASSWORD,
		wlist,"/home/jlingys/pyGenerator/log")
