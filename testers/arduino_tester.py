import socket
import time
import threading
import json

class RobotController:
    def __init__(self, host="192.168.4.1", port=80):
        """
        Initialize the robot controller
        host: IP address of your ESP32 (default is ESP32's AP mode IP)
        port: Port number (80 for HTTP, 100 for TCP)
        """
        self.host = host
        self.port = port
        self.sock = None
        self.connected = False
        self.timeout = 5  # seconds
        
    def connect(self):
        """Connect to the robot"""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(self.timeout)
            self.sock.connect((self.host, self.port))
            self.connected = True
            print(f"Connected to robot at {self.host}:{self.port}")
            return True
        except Exception as e:
            print(f"Connection failed: {e}")
            return False
    
    def send_command(self, command, params=None):
        """Send a command to the robot"""
        if not self.connected:
            print("Not connected to robot")
            return None
            
        try:
            # Build the command string
            if params is None:
                cmd_str = f"{command}:"
            else:
                cmd_str = f"{command}:{params}"
            
            # Send command
            self.sock.sendall((cmd_str + "\n").encode('utf-8'))
            
            # Wait for response
            response = ""
            start_time = time.time()
            
            while time.time() - start_time < self.timeout:
                try:
                    data = self.sock.recv(1024).decode('utf-8')
                    if not data:
                        break
                    response += data
                    
                    # Check if we have a complete response
                    if '\n' in response:
                        # Process all complete lines
                        lines = response.split('\n')
                        response = lines[-1]  # Keep incomplete line
                        
                        for line in lines[:-1]:
                            if line.strip():
                                print(f"Response: {line.strip()}")
                                return line.strip()
                        
                except socket.timeout:
                    break
                except Exception as e:
                    print(f"Error receiving data: {e}")
                    break
            
            # If we timed out, return None
            if not response:
                print("No response received")
                return None
                
            # Return the last incomplete line
            return response
            
        except Exception as e:
            print(f"Error sending command: {e}")
            return None
    
    def move_forward(self, speed=180):
        """Move forward with given speed"""
        return self.send_command("move", f"forward,{speed}")
    
    def move_backward(self, speed=180):
        """Move backward with given speed"""
        return self.send_command("move", f"backward,{speed}")
    
    def move_left(self, speed=180):
        """Move left with given speed"""
        return self.send_command("move", f"left,{speed}")
    
    def move_right(self, speed=180):
        """Move right with given speed"""
        return self.send_command("move", f"right,{speed}")
    
    def move_stop(self):
        """Stop the robot"""
        return self.send_command("move", "stop")
    
    def move_clockwise(self, speed=180):
        """Rotate clockwise"""
        return self.send_command("move", f"clockwise,{speed}")
    
    def move_counter_clockwise(self, speed=180):
        """Rotate counter-clockwise"""
        return self.send_command("move", f"counter-clockwise,{speed}")
    
    def set_servo_angle(self, angle):
        """Set servo angle (0-180)"""
        return self.send_command("servo", str(angle))
    
    def trigger_shoot(self, duration=200):
        """Trigger shooting with given duration"""
        return self.send_command("shoot", str(duration))
    
    def play_buzzer(self, tone=1000, duration=1000):
        """Play buzzer with given tone and duration"""
        return self.send_command("buzzer", f"{tone},{duration}")
    
    def toggle_led1(self):
        """Toggle LED 1"""
        return self.send_command("led", "toggle1")
    
    def toggle_led2(self):
        """Toggle LED 2"""
        return self.send_command("led", "toggle2")
    
    def set_led1(self, state):
        """Set LED 1 state (on/off)"""
        return self.send_command("led", "on" if state else "off")
    
    def set_led2(self, state):
        """Set LED 2 state (on/off)"""
        return self.send_command("led", "on" if state else "off")
    
    def start_camera(self):
        """Start camera streaming"""
        return self.send_command("camera", "start")
    
    def stop_camera(self):
        """Stop camera streaming"""
        return self.send_command("camera", "stop")
    
    def start_tracking(self, mode=1):
        """Start tracking mode"""
        return self.send_command("track", str(mode))
    
    def start_avoidance(self):
        """Start obstacle avoidance"""
        return self.send_command("avoid", "1")
    
    def start_following(self):
        """Start following mode"""
        return self.send_command("follow", "1")
    
    def stop_all(self):
        """Stop all operations"""
        return self.send_command("stop")
    
    def get_status(self):
        """Get robot status"""
        return self.send_command("getStatus")
    
    def get_capabilities(self):
        """Get available commands"""
        return self.send_command("getCapabilities")
    
    def get_sensor_data(self):
        """Get sensor readings"""
        return self.send_command("getSensorData")
    
    def close(self):
        """Close the connection"""
        if self.sock:
            self.sock.close()
            self.connected = False
            print("Connection closed")

def demo_robot_control():
    """Demonstrate robot control with a simple sequence"""
    # Create robot controller
    robot = RobotController(host="192.168.4.1", port=80)
    
    # Connect to robot
    if not robot.connect():
        print("Failed to connect to robot")
        return
    
    try:
        # Get robot capabilities
        print("\n=== Getting robot capabilities ===")
        capabilities = robot.get_capabilities()
        if capabilities:
            print(f"Capabilities: {capabilities}")
        
        # Get initial status
        print("\n=== Getting initial status ===")
        status = robot.get_status()
        if status:
            print(f"Status: {status}")
        
        # Get sensor data
        print("\n=== Getting sensor data ===")
        sensor_data = robot.get_sensor_data()
        if sensor_data:
            print(f"Sensor data: {sensor_data}")
        
        # Simple control demo
        print("\n=== Starting control demo ===")
        
        # Move forward
        robot.move_forward(180)
        time.sleep(2)
        
        # Stop
        robot.move_stop()
        time.sleep(1)
        
        # Move left
        robot.move_left(180)
        time.sleep(2)
        
        # Stop
        robot.move_stop()
        time.sleep(1)
        
        # Move right
        robot.move_right(180)
        time.sleep(2)
        
        # Stop
        robot.move_stop()
        time.sleep(1)
        
        # Rotate clockwise
        robot.move_clockwise(180)
        time.sleep(2)
        
        # Stop
        robot.move_stop()
        time.sleep(1)
        
        # Rotate counter-clockwise
        robot.move_counter_clockwise(180)
        time.sleep(2)
        
        # Stop
        robot.move_stop()
        time.sleep(1)
        
        # Test servos
        print("\n=== Testing servos ===")
        for angle in [0, 90, 180, 90]:
            robot.set_servo_angle(angle)
            time.sleep(1)
        
        # Test buzzer
        print("\n=== Testing buzzer ===")
        robot.play_buzzer(1000, 500)
        time.sleep(1)
        
        # Test LEDs
        print("\n=== Testing LEDs ===")
        robot.set_led1(True)
        time.sleep(1)
        robot.set_led1(False)
        time.sleep(1)
        
        robot.set_led2(True)
        time.sleep(1)
        robot.set_led2(False)
        time.sleep(1)
        
        # Test shooting
        print("\n=== Testing shooting ===")
        robot.trigger_shoot(200)
        time.sleep(1)
        
        # Test tracking
        print("\n=== Testing tracking ===")
        robot.start_tracking(1)
        time.sleep(3)
        robot.stop_all()
        
        # Test obstacle avoidance
        print("\n=== Testing obstacle avoidance ===")
        robot.start_avoidance()
        time.sleep(3)
        robot.stop_all()
        
        # Test following
        print("\n=== Testing following ===")
        robot.start_following()
        time.sleep(3)
        robot.stop_all()
        
        # Get final status
        print("\n=== Getting final status ===")
        final_status = robot.get_status()
        if final_status:
            print(f"Final status: {final_status}")
            
    except Exception as e:
        print(f"Error during demo: {e}")
    
    finally:
        # Clean up
        robot.close()
        print("\nDemo complete")

def interactive_control():
    """Interactive control interface"""
    robot = RobotController(host="192.168.4.1", port=80)
    
    if not robot.connect():
        print("Failed to connect to robot")
        return
    
    try:
        print("\n=== Interactive Robot Control ===")
        print("Commands: move_forward, move_backward, move_left, move_right")
        print("          move_stop, move_clockwise, move_counter_clockwise")
        print("          set_servo_angle(angle), trigger_shoot(duration)")
        print("          play_buzzer(tone,duration), set_led1(state), set_led2(state)")
        print("          start_tracking(mode), start_avoidance(), start_following()")
        print("          get_status(), get_sensor_data(), get_capabilities()")
        print("          quit to exit")
        
        while True:
            command = input("\nEnter command: ").strip().lower()
            
            if command == "quit":
                break
            elif command == "help":
                print("\nAvailable commands:")
                print("  move_forward [speed] - Move forward")
                print("  move_backward [speed] - Move backward")
                print("  move_left [speed] - Turn left")
                print("  move_right [speed] - Turn right")
                print("  move_stop - Stop")
                print("  move_clockwise [speed] - Rotate clockwise")
                print("  move_counter_clockwise [speed] - Rotate counter-clockwise")
                print("  set_servo_angle angle - Set servo angle (0-180)")
                print("  trigger_shoot duration - Trigger shooting")
                print("  play_buzzer tone,duration - Play buzzer")
                print("  set_led1 state - Turn LED1 on/off")
                print("  set_led2 state - Turn LED2 on/off")
                print("  start_tracking mode - Start tracking")
                print("  start_avoidance - Start obstacle avoidance")
                print("  start_following - Start following mode")
                print("  get_status - Get robot status")
                print("  get_sensor_data - Get sensor data")
                print("  get_capabilities - Get available commands")
                print("  quit - Exit")
            elif command.startswith("move_forward"):
                speed = 180
                if len(command.split()) > 1:
                    speed = int(command.split()[1])
                robot.move_forward(speed)
            elif command.startswith("move_backward"):
                speed = 180
                if len(command.split()) > 1:
                    speed = int(command.split()[1])
                robot.move_backward(speed)
            elif command.startswith("move_left"):
                speed = 180
                if len(command.split()) > 1:
                    speed = int(command.split()[1])
                robot.move_left(speed)
            elif command.startswith("move_right"):
                speed = 180
                if len(command.split()) > 1:
                    speed = int(command.split()[1])
                robot.move_right(speed)
            elif command == "move_stop":
                robot.move_stop()
            elif command.startswith("move_clockwise"):
                speed = 180
                if len(command.split()) > 1:
                    speed = int(command.split()[1])
                robot.move_clockwise(speed)
            elif command.startswith("move_counter_clockwise"):
                speed = 180
                if len(command.split()) > 1:
                    speed = int(command.split()[1])
                robot.move_counter_clockwise(speed)
            elif command.startswith("set_servo_angle"):
                angle = int(command.split()[1])
                robot.set_servo_angle(angle)
            elif command.startswith("trigger_shoot"):
                duration = 200
                if len(command.split()) > 1:
                    duration = int(command.split()[1])
                robot.trigger_shoot(duration)
            elif command.startswith("play_buzzer"):
                parts = command.split()[1].split(',')
                tone = int(parts[0])
                duration = int(parts[1])
                robot.play_buzzer(tone, duration)
            elif command.startswith("set_led1"):
                state = command.split()[1].lower()
                robot.set_led1(state == "on")
            elif command.startswith("set_led2"):
                state = command.split()[1].lower()
                robot.set_led2(state == "on")
            elif command == "start_tracking":
                robot.start_tracking(1)
            elif command == "start_avoidance":
                robot.start_avoidance()
            elif command == "start_following":
                robot.start_following()
            elif command == "get_status":
                robot.get_status()
            elif command == "get_sensor_data":
                robot.get_sensor_data()
            elif command == "get_capabilities":
                robot.get_capabilities()
            else:
                print("Unknown command. Type 'help' for available commands.")
                
    except KeyboardInterrupt:
        print("\nExiting...")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        robot.close()

if __name__ == "__main__":
    # Choose which mode to run
    choice = input("Choose mode (1: Demo, 2: Interactive): ").strip()
    
    if choice == "1":
        demo_robot_control()
    elif choice == "2":
        interactive_control()
    else:
        print("Invalid choice. Running demo mode.")
        demo_robot_control()