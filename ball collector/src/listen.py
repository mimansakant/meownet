import socket

UDP_IP = "0.0.0.0" # Listen on all interfaces
UDP_PORT = 5005

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

print(f"Listening for ESP32 on port {UDP_PORT}...")

while True:
    data, addr = sock.recvfrom(1024)
    msg = data.decode('utf-8')
    
    if "[TELEMETRY]" in msg:
        print(f"\033[93m{msg}\033[0m") # Yellow for telemetry
    elif "MODE" in msg:
        print(f"\033[92m--- {msg} ---\033[0m") # Green for Mode
    else:
        print(f"\033[94mHIT: {msg}\033[0m") # Blue for Hits