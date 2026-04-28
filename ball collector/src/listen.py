import socket

UDP_IP = ""       # listen on all interfaces
UDP_PORT = 5005

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

print("Listening for ball tracker messages...")
while True:
    data, addr = sock.recvfrom(1024)
    print(f"[{addr[0]}] {data.decode()}")