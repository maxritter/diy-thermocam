from src.liveviewer import LiveViewer
import serial.tools.list_ports as ports

# print all the available COM ports to the terminal so that its easy to find the correct port
com_ports = list(ports.comports()) 
for i in com_ports:            
    print(i.device) 

SERIAL_PORT = "COM10"

if __name__ == "__main__":
    LiveViewer(SERIAL_PORT).run()
