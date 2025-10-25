import serial
import time

ser = serial.Serial('/dev/ttyUSB0', 115200)  # Adjust port
time.sleep(2)

ser.write(b"snapshot:\n")

with open("image.jpg", "wb") as f:
    start = False
    while True:
        byte = ser.read(1)
        if not byte:
            continue
        if byte == b'S':
            start = True
            print("Starting image capture...")
        elif byte == b'E':
            print("Image capture complete.")
            break
        elif start:
            f.write(byte)

print("Image saved as image.jpg")