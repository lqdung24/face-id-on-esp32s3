import serial
import base64

# Thay cổng /dev/ttyUSB0 bằng cổng thực tế đang cắm mạch của bạn
PORT = '/dev/ttyACM0'
BAUD = 115200

try:
    ser = serial.Serial(PORT, BAUD)
    print(f"Đang lắng nghe camera trên cổng {PORT}...")
    
    is_reading = False
    b64_data = []

    while True:
        # Đọc từng dòng từ mạch gửi lên
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        
        if "---BEGIN_IMAGE---" in line:
            is_reading = True
            b64_data = []
            print("Đang tải khung hình...")
            
        elif "---END_IMAGE---" in line:
            is_reading = False
            try:
                # Giải mã và lưu thành file JPG
                img_bytes = base64.b64decode("".join(b64_data))
                with open("camera_view.jpg", "wb") as f:
                    f.write(img_bytes)
                print("Đã lưu thành công file camera_view.jpg! Hãy mở ảnh lên để kiểm tra.")
            except Exception as e:
                print("Lỗi giải mã ảnh:", e)
                
        elif is_reading:
            b64_data.append(line)
            
except Exception as e:
    print("Không thể mở cổng Serial. Hãy chắc chắn bạn đã tắt 'idf.py monitor'. Lỗi:", e)