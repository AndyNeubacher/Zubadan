import os
import sys
import time
import serial

logfile = "/home/python/log.txt"
port = "/dev/ttyS1"
baud = 2400
char_timeout_ms = 40





time_start = 0
time_last_char = 0
fd = open(logfile,'a+')
ser = serial.Serial(port, baud)



def log(msg, type):
	global time_start
	global time_last_char
	global fd

	#check if the log is called the first time
	if time_start:
		time_now = int(round(time.time()*1000))
		time_delta = int(time_now - time_start)
		if (time_now - time_last_char) < char_timeout_ms:
			log_str = "".join((" ", msg))
		else:
			log_str = "".join(("\n", str(time_delta), ": ", msg))
			
		time_last_char = time_now
	else:
		time_start = int(round(time.time()*1000))
		time_last_char = time_start
		log_str = "".join(("0: ", msg))

	
	#log to CLI or to FILE
	if type == "FILE":
		fd.write(log_str)
	else:
		sys.stdout.write(log_str)
		#print log_str

		
	
		
		
		
		
log("Start Logging", "CLI")
os.nice(20)

while 1:
	val = ser.read()
	log(val.encode('hex'), "FILE")

