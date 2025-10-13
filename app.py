import sys
from assistant import Assistant

if __name__ == "__main__":
        console_mode = "--console" in sys.argv or True # Automatically setting to console for now
        assistant = Assistant(console_mode=console_mode)
        assistant.run()
