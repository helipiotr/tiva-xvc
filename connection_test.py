#!/usr/bin/env python

import socket


TCP_IP = '192.168.0.45'
TCP_PORT = 2542
BUFFER_SIZE = 1024
info = "getinfo:"
print("getinfo:\n")

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((TCP_IP, TCP_PORT))

s.send(info.encode())
data = s.recv(BUFFER_SIZE)
print("\t received data:", data)

message = "shift:".encode()

nr_bits = bytearray([8,0,0,0]) #little endian!

message += nr_bits

tms = bytearray.fromhex("CA")#bytearray(["CA".decode("hex")])
tdi = bytearray.fromhex("AC")#bytearray(["AC".decode("hex")])

message+=tms
message+=tdi

print("Sending TDI/TMS")

s.send(message)

data = s.recv(BUFFER_SIZE)

print("\t received data:", data)

s.close()


