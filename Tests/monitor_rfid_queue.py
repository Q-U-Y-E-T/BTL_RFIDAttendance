#!/usr/bin/env python3
"""UART monitor for the RFID task/queue test, 115200 baud, 8N1."""
import argparse
import sys

try:
    import serial
except ImportError:
    sys.exit("Cai pyserial: sudo apt install python3-serial")

parser = argparse.ArgumentParser()
parser.add_argument("--port", required=True)
args = parser.parse_args()
try:
    with serial.Serial(args.port, 115200, timeout=0.2,
                       xonxoff=False, rtscts=False, dsrdtr=False) as port:
        print("Listening. Nhan RESET tren STM32. Ctrl+C de thoat.", flush=True)
        while True:
            data = port.read(256)
            if data:
                print(data.decode("utf-8", errors="replace"), end="", flush=True)
except KeyboardInterrupt:
    pass
except serial.SerialException as error:
    sys.exit(str(error))
