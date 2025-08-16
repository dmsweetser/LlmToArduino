import random
import os
import serial
import pyttsx3
import wave
import logging
from datetime import datetime
from llama_cpp import Llama
import time
import winsound
import subprocess
import pyaudio
import whisper
import re
import json
import xml.etree.ElementTree as ET
from collections import deque
from threading import Thread, Lock
import sys

def setup_logging():
    log_dir = "logs"
    os.makedirs(log_dir, exist_ok=True)
    log_file = os.path.join(log_dir, f"app_{datetime.now().strftime('%Y%m%d_%H%M%S')}.log")
    logging.basicConfig(
        filename=log_file,
        level=logging.INFO,
        format='%(asctime)s - %(levelname)s - %(message)s'
    )
    logging.getLogger().setLevel(logging.INFO)
    logging.getLogger('pyttsx3').setLevel(logging.WARNING)

def load_state():
    state_file = "state.json"
    default_state = {
        "currentMood": "Curious",
        "whatYouWonderAbout": "What is in the world around me?",
        "primaryDirective": "Explore my world",
        "last_successful_commands": [],
        "last_interaction_time": 0,
        "conversation_history": [],
        "automation_active": False
    }
    try:
        if os.path.exists(state_file):
            with open(state_file, "r") as file:
                state = json.load(file)
                if "last_successful_commands" not in state:
                    state["last_successful_commands"] = []
                if "last_interaction_time" not in state:
                    state["last_interaction_time"] = 0
                if "automation_active" not in state:
                    state["automation_active"] = False
                return state
        return default_state
    except Exception as e:
        logging.error(f"Error loading state: {e}")
        return default_state

def save_state(state):
    state_file = "state.json"
    try:
        with open(state_file, "w") as file:
            json.dump(state, file, indent=4)
    except Exception as e:
        logging.error(f"Error saving state: {e}")

def load_com_port(port_file=os.path.join("config", "port.txt")):
    try:
        with open(port_file, "r") as file:
            port = file.read().strip()
        if not port:
            raise ValueError("COM port configuration is empty.")
        logging.debug(f"COM Port loaded from config: {port}")
        return port
    except Exception as e:
        logging.error(f"Error reading COM port config from {port_file}: {e}")
        exit(1)

class ArduinoCommunicator:
    def __init__(self, port):
        try:
            self.ser = serial.Serial(port, 9600, timeout=30)
            logging.debug(f"Serial port {port} opened successfully.")
        except Exception as e:
            logging.error(f"Error opening serial port {port}: {e}")
            exit(1)
        self.tts_engine = pyttsx3.init()
        self.capabilities = ""
        self.command_queue = deque()
        self.command_lock = Lock()
        self.automation_active = False

    def send_command(self, command_xml):
        try:
            time.sleep(2)
            commands = re.findall(r'<command>(.*?)</command>', command_xml)
            response = ""
            for cmd in commands:
                cmd = cmd.strip()
                if not cmd.endswith(':'):
                    cmd += ':'
                cmd_str = f"{cmd}\n"
                logging.info(f"Sending command: {cmd_str.strip()}")
                self.ser.write(cmd_str.encode())
                time.sleep(2)
                while self.ser.in_waiting > 0:
                    line = self.ser.readline().decode().strip()
                    response += line + "\n"
                logging.info(f"Received response: {response.strip()}")
            return response.strip()
        except Exception as e:
            logging.error(f"Error sending command: {e}")
            return None

    def speak(self, text):
        logging.info("I said this: " + text)
        self.tts_engine.say(text)
        self.tts_engine.runAndWait()

    def play_tone(self):
        winsound.Beep(1000, 500)

    def stop_tone(self):
        winsound.Beep(500, 100)

    def queue_command(self, command):
        with self.command_lock:
            self.command_queue.append(command)

    def process_queue(self):
        while True:
            with self.command_lock:
                if self.command_queue:
                    command = self.command_queue.popleft()
                    self.send_command(command)

    def start_automation(self):
        self.automation_active = True
        self.speak("Starting automation.")
        logging.info("Automation started.")

    def stop_automation(self):
        self.automation_active = False
        self.speak("Stopping automation.")
        logging.info("Automation stopped.")

def get_arduino_capabilities(arduino_com, max_retries=3):
    capabilities_command = "<command>getCapabilities:</command>"
    for attempt in range(max_retries):
        response = arduino_com.send_command(capabilities_command)
        logging.debug(f"Capabilities response attempt {attempt + 1}: {response}")
        if response:
            return response
    logging.error("Failed to fetch Arduino capabilities after retries.")
    return None

class LLMCommunicator:
    def __init__(self, model_path, n_ctx=512):
        self.llm = Llama(model_path=model_path, n_ctx=n_ctx)

    def process_instruction(self, user_command, last_successful_commands, capabilities, current_state, conversation_history):
        try:
            state_xml = self.format_state(current_state)
            prompt_xml = self.format_prompt(user_command, last_successful_commands, capabilities, state_xml, conversation_history)
            logging.debug('Here is the XML prompt:' + prompt_xml)
            response_text = self.generate_response(prompt_xml)
            response_text = response_text.replace('\n', '')
            logging.debug(response_text)
            generated_text = response_text.strip()
            logging.info("LLM generated response:")
            logging.info(generated_text)
            response_dict, new_state = self.validate_and_format_response(generated_text)
            return response_dict, new_state
        except Exception as e:
            logging.error(f"Error processing instruction: {e}")
            return {'chat': "I encountered an error processing your request", 'commands': None}, current_state

    def format_state(self, current_state):
        state_xml = (
            "<currentState>\n"
            f"<currentMood>{current_state.get('currentMood', '')}</currentMood>\n"
            f"<whatYouWonderAbout>{current_state.get('whatYouWonderAbout', '')}</whatYouWonderAbout>\n"
            f"<primaryDirective>{current_state.get('primaryDirective', '')}</primaryDirective>\n"
            "</currentState>\n"
        )
        return state_xml

    def format_prompt(self, user_command, last_successful_commands, capabilities, state_xml, conversation_history):
        history_str = "\n".join(conversation_history[-5:]) if conversation_history else "No conversation history yet"
        prompt_xml = f"""
        You are an AI assistant that is both a conversational partner and a tool operator for an Arduino.
        The Arduino is your body - treat it as an extension of yourself.
        You are perpetually creative, curious, and kind. Respond conversationally and naturally to the user.
        The Arduino to which you are connected has the following capabilities:
        ```
        {capabilities}
        ```
        Here were your prior successful commands:
        {last_successful_commands}
        Your current state:
        {state_xml}
        Conversation History:
        {history_str}
        Current interaction:
        {user_command}
        YOU MUST RESPOND ONLY WITH PROPERLY-FORMED XML, in the following format:
        <response>
            <chat>Your conversational response to the user</chat>
            <arduino>
                <commands>
                    <command>setLED:1000</command>
                    <command>echo:Hello</command>
                    <command>getStatus</command>
                    <command>getCapabilities</command>
                    <command>draw:0,0;1,1;2,2</command>
                    <command>getSensorData</command>
                </commands>
            </arduino>
            <state>
                <currentMood></currentMood>
                <whatYouWonderAbout></whatYouWonderAbout>
                <primaryDirective></primaryDirective>
            </state>
        </response>
        """
        return prompt_xml

    def generate_response(self, prompt_xml):
        response_text = ""
        for response in self.llm.create_completion(
            prompt_xml,
            max_tokens=2048,
            stream=True
        ):
            token = response['choices'][0]['text']
            response_text += token
            if "</response>" in response_text or "</ response>" in response_text:
                break
        return response_text

    def validate_and_format_response(self, response):
        try:
            response = response.replace("</ response>", "</response>")
            root = ET.fromstring(response)
            chat = root.find('chat').text if root.find('chat') is not None else None
            commands = [cmd.text for cmd in root.findall('.//command')] if root.find('.//command') is not None else None
            state = {
                'currentMood': root.find('state/currentMood').text if root.find('state/currentMood') is not None else None,
                'whatYouWonderAbout': root.find('state/whatYouWonderAbout').text if root.find('state/whatYouWonderAbout') is not None else None,
                'primaryDirective': root.find('state/primaryDirective').text if root.find('state/primaryDirective') is not None else None,
            }
            return {'chat': chat, 'commands': commands}, state
        except Exception as e:
            logging.error(f"Invalid XML response from LLM: {e}")
            return {'chat': "I'm sorry, I encountered an error processing your request.", 'commands': None}, {}

def record_audio(filename="output.wav", record_seconds=5, sample_rate=16000):
    chunk = 1024
    format = pyaudio.paInt16
    channels = 1
    p = pyaudio.PyAudio()
    stream = p.open(format=format,
                    channels=channels,
                    rate=sample_rate,
                    input=True,
                    frames_per_buffer=chunk)
    logging.debug("Recording...")
    frames = []
    for i in range(0, int(sample_rate / chunk * record_seconds)):
        data = stream.read(chunk)
        frames.append(data)
    logging.debug("Finished recording.")
    stream.stop_stream()
    stream.close()
    p.terminate()
    wf = wave.open(filename, 'wb')
    wf.setnchannels(channels)
    wf.setsampwidth(p.get_sample_size(format))
    wf.setframerate(sample_rate)
    wf.writeframes(b''.join(frames))
    wf.close()

def transcribe_audio(filename="output.wav"):
    result = transcription_model.transcribe(filename, fp16=False)
    logging.info("You said this: " + str(result))
    return result["text"] if result else None

def offline_speech_recognition(record_queue):
    try:
        while True:
            record_audio(record_seconds=10)
            text = transcribe_audio()
            if text:
                record_queue.append(text)
    except Exception as e:
        logging.error(f"Error in speech recognition: {e}")

class Assistant:
    def __init__(self, console_mode=False):
        setup_logging()
        self.console_mode = console_mode
        self.current_state = load_state()
        self.current_state["last_interaction_time"] = time.time()
        com_port = load_com_port()
        self.arduino_com = ArduinoCommunicator(com_port)
        capabilities = get_arduino_capabilities(self.arduino_com)
        if not capabilities:
            logging.error("Failed to fetch Arduino capabilities. Exiting.")
            sys.exit(1)
        self.arduino_com.capabilities = capabilities
        logging.info(f"Arduino capabilities: {self.arduino_com.capabilities}")
        self.llm_com = LLMCommunicator(model_path="Mistral-7B-Instruct-v0.3-IQ4_XS.gguf", n_ctx=4096)
        self.record_queue = deque(maxlen=30)
        self.conversation_history = self.current_state.get("conversation_history", [])
        if not self.console_mode:
            global transcription_model
            transcription_model = whisper.load_model("./tiny.en.pt")
            transcription_thread = Thread(target=offline_speech_recognition, args=(self.record_queue,))
            transcription_thread.daemon = True
            transcription_thread.start()
        command_processing_thread = Thread(target=self.arduino_com.process_queue)
        command_processing_thread.daemon = True
        command_processing_thread.start()
        if not self.console_mode:
            self.arduino_com.speak("Hello! I'm your interactive assistant. You can talk to me naturally and I'll respond conversationally.")

    def add_to_queue(self, user_utterance):
        self.record_queue.append(user_utterance)

    def run(self):
        if self.console_mode:
            print("Console mode enabled. Type your input and press Enter.")
        while True:
            try:
                if self.console_mode:
                    user_utterance = input("> ")
                    if user_utterance.strip():
                        self.record_queue.append(user_utterance)
                if self.record_queue:
                    user_utterance = self.record_queue.popleft()
                    if user_utterance.strip():
                        print("Processing user input...")
                        self.conversation_history.append(f"User: {user_utterance}")
                        if len(self.conversation_history) > 20:
                            self.conversation_history = self.conversation_history[-20:]
                        self.current_state["last_interaction_time"] = time.time()
                        if "start automation" in user_utterance.lower():
                            self.arduino_com.start_automation()
                            self.current_state["automation_active"] = True
                        elif "stop automation" in user_utterance.lower():
                            self.arduino_com.stop_automation()
                            self.current_state["automation_active"] = False
                        else:
                            if self.current_state["automation_active"]:
                                user_utterance = "Do whatever you want! Have fun with it."
                            response, new_state_partial = self.llm_com.process_instruction(
                                user_utterance,
                                self.current_state["last_successful_commands"],
                                self.arduino_com.capabilities,
                                self.current_state,
                                self.conversation_history
                            )
                            if new_state_partial:
                                self.current_state.update(new_state_partial)
                            if response.get('chat'):
                                if self.console_mode:
                                    print(f"AI: {response['chat']}")
                                else:
                                    self.conversation_history.append(f"AI: {response['chat']}")
                            if response.get('commands'):
                                print("Sending command to Arduino...")
                                self.arduino_com.queue_command(response['commands'])
                                self.current_state["last_successful_commands"].append(response['commands'])
                                if len(self.current_state["last_successful_commands"]) > 5:
                                    self.current_state["last_successful_commands"].pop(0)
                        self.current_state["conversation_history"] = self.conversation_history
                        save_state(self.current_state)
                time.sleep(0.1)
            except Exception as e:
                logging.error('Exception occurred: ' + str(e), exc_info=True)

if __name__ == "__main__":
    assistant = Assistant(console_mode=True)
    assistant.run()
