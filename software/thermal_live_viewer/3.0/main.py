from src.liveviewer import LiveViewer

SERIAL_PORT = "COM10"

if __name__ == "__main__":
    LiveViewer(SERIAL_PORT).run()
