import socket
import sys
import os
import subprocess

FNULL = open(os.devnull,'w')

PORT = 6666

def main():
	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	s.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
	print 'Socket created'

	try:
		s.bind(('',PORT))
	except socket.error as msg:
		print 'Bind failed. Error code: '+str(msg[0])+ ' Message ' + msg[1]
		sys.exit()
	print 'socket bind complete'

	s.listen(10)
	while(1):
		conn, addr = s.accept()
		print 'Connected..'

		while 1:
			res = conn.recv(1024)
			if res == '1':
				print 'recieve start attack'
				P = subprocess.Popen(["/home/attacker/YakirNaamaProject/CseProject-FindPattern/FillingTheCache/FillingTheCache"],stdout=FNULL,stderr=subprocess.STDOUT)
			elif res== '2':
				print 'recieve stop attack'
				P.kill()
				break;
	
	
		


if __name__ == "__main__":
	main()
