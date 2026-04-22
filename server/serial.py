import serial

# chỉnh lại đúng port của bạn
PORT = "/dev/ttyACM0"   # Linux
# PORT = "COM3"         # Windows

BAUDRATE = 115200

ser = serial.Serial(PORT, BAUDRATE, timeout=1)

print(f"Listening on {PORT} @ {BAUDRATE}...\n")

try:
    while True:
        if ser.in_waiting > 0:
            line = ser.readline().decode(errors='ignore').strip()
            if line:
                print(line)

except KeyboardInterrupt:
    print("\nExit")

finally:
    ser.close()