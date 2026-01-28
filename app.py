import random
import os
import serial
import pyttsx3
import wave
import logging
from datetime import datetime
from llama_cpp import Llama
import time
import subprocess
import pyaudio
import whisper
import re
import json
import xml.etree.ElementTree as ET
from collections import deque
from threading import Thread, Lock
import sys
from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple, Any
from enum import Enum, auto

class LogLevel(Enum):
    INFO = auto()
    WARNING = auto()
    ERROR = auto()

class MemoryDomain(Enum):
    CORE_MEMORY = "Core Memory"
    SHORT_TERM_MEMORY = "Short Term Memory"
    PROCEDURAL_MEMORY = "Procedural Memory"
    EPISODIC_MEMORY = "Episodic Memory"
    SENSORY_MEMORY = "Sensory Memory"

@dataclass
class MemoryEntry:
    summary: str
    details: Dict[str, Any]
    timestamp: float
    confidence: float = 1.0

@dataclass
class MemoryDomainData:
    domain: MemoryDomain
    entries: List[MemoryEntry]

@dataclass
class ComplexMemory:
    domains: Dict[MemoryDomain, MemoryDomainData]

class LoggingSystem:
    """Handles all logging operations with functional approach"""

    @staticmethod
    def setup_logging() -> None:
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

    @staticmethod
    def log(message: str, level: LogLevel = LogLevel.INFO) -> None:
        if level == LogLevel.INFO:
            logging.info(message)
        elif level == LogLevel.WARNING:
            logging.warning(message)
        elif level == LogLevel.ERROR:
            logging.error(message)

class StateManager:
    """Manages application state with complex memory system"""

    def __init__(self) -> None:
        self.state_file = "state.json"
        self.default_state = {
            "current_mood": "Curious",
            "current_wonder": "What is in the world around me?",
            "primary_directive": "Explore my world",
            "last_successful_commands": [],
            "last_interaction_time": 0,
            "conversation_history": [],
            "automation_active": False,
            "memory": self._initialize_memory()
        }
        self.current_state = self.load_state()

    def _initialize_memory(self) -> ComplexMemory:
        return ComplexMemory({
            MemoryDomain.CORE_MEMORY: MemoryDomainData(
                domain=MemoryDomain.CORE_MEMORY,
                entries=[]
            ),
            MemoryDomain.SHORT_TERM_MEMORY: MemoryDomainData(
                domain=MemoryDomain.SHORT_TERM_MEMORY,
                entries=[]
            ),
            MemoryDomain.PROCEDURAL_MEMORY: MemoryDomainData(
                domain=MemoryDomain.PROCEDURAL_MEMORY,
                entries=[]
            ),
            MemoryDomain.EPISODIC_MEMORY: MemoryDomainData(
                domain=MemoryDomain.EPISODIC_MEMORY,
                entries=[]
            ),
            MemoryDomain.SENSORY_MEMORY: MemoryDomainData(
                domain=MemoryDomain.SENSORY_MEMORY,
                entries=[]
            )
        })

    def load_state(self) -> Dict[str, Any]:
        try:
            if os.path.exists(self.state_file):
                with open(self.state_file, "r") as file:
                    state = json.load(file)
                    # Ensure all required fields exist
                    state.setdefault("last_successful_commands", [])
                    state.setdefault("last_interaction_time", 0)
                    state.setdefault("automation_active", False)
                    state.setdefault("memory", self._initialize_memory())
                    return state
            return self.default_state
        except Exception as e:
            LoggingSystem.log(f"Error loading state: {e}", LogLevel.ERROR)
            return self.default_state

    def save_state(self) -> None:
        try:
            with open(self.state_file, "w") as file:
                json.dump(self.current_state, file, indent=4, default=self._serialize_memory)
        except Exception as e:
            LoggingSystem.log(f"Error saving state: {e}", LogLevel.ERROR)

    def _serialize_memory(self, obj: Any) -> Any:
        if isinstance(obj, MemoryDomain):
            return obj.value
        elif isinstance(obj, MemoryDomainData):
            return {
                "domain": obj.domain.value,
                "entries": [{
                    "summary": entry.summary,
                    "details": entry.details,
                    "timestamp": entry.timestamp,
                    "confidence": entry.confidence
                } for entry in obj.entries]
            }
        elif isinstance(obj, ComplexMemory):
            return {
                "domains": {
                    domain.value: self._serialize_memory(data)
                    for domain, data in obj.domains.items()
                }
            }
        raise TypeError(f"Object of type {type(obj)} is not JSON serializable")

    def add_memory_entry(self, domain: MemoryDomain, summary: str, details: Dict[str, Any], confidence: float = 1.0) -> None:
        entry = MemoryEntry(
            summary=summary,
            details=details,
            timestamp=time.time(),
            confidence=confidence
        )
        self.current_state["memory"].domains[domain].entries.append(entry)
        # Maintain memory size limits
        self._maintain_memory_size(domain)

    def _maintain_memory_size(self, domain: MemoryDomain) -> None:
        max_entries = {
            MemoryDomain.CORE_MEMORY: 100,
            MemoryDomain.SHORT_TERM_MEMORY: 50,
            MemoryDomain.PROCEDURAL_MEMORY: 100,
            MemoryDomain.EPISODIC_MEMORY: 200,
            MemoryDomain.SENSORY_MEMORY: 100
        }
        current_entries = self.current_state["memory"].domains[domain].entries
        if len(current_entries) > max_entries[domain]:
            # Remove oldest entries first
            current_entries.sort(key=lambda x: x.timestamp)
            self.current_state["memory"].domains[domain].entries = current_entries[-max_entries[domain]:]

    def get_memory_summary(self, domain: MemoryDomain, max_entries: int = 5) -> List[Dict[str, Any]]:
        entries = self.current_state["memory"].domains[domain].entries
        entries.sort(key=lambda x: x.timestamp, reverse=True)
        return [{
            "summary": entry.summary,
            "timestamp": entry.timestamp,
            "confidence": entry.confidence
        } for entry in entries[:max_entries]]

class HardwareCommunicator:
    """Handles direct hardware communication with functional approach"""

    def __init__(self, port: str) -> None:
        try:
            self.serial_connection = serial.Serial(port, 9600, timeout=30)
            LoggingSystem.log(f"Serial port {port} opened successfully.", LogLevel.INFO)
            self.capabilities = ""
        except Exception as e:
            LoggingSystem.log(f"Error opening serial port {port}: {e}", LogLevel.ERROR)
            sys.exit(1)

    def send_command(self, command_xml: str) -> Optional[str]:
        try:
            time.sleep(2)
            commands = re.findall(r'<command>(.*?)</command>', command_xml)
            response = ""
            for cmd in commands:
                cmd = cmd.strip()
                if not cmd.endswith(':'):
                    cmd += ':'
                cmd_str = f"{cmd}\n"
                LoggingSystem.log(f"Sending command: {cmd_str.strip()}", LogLevel.INFO)
                self.serial_connection.write(cmd_str.encode())
                time.sleep(2)
                while self.serial_connection.in_waiting > 0:
                    line = self.serial_connection.readline().decode().strip()
                    response += line + "\n"
            LoggingSystem.log(f"Received response: {response.strip()}", LogLevel.INFO)
            return response.strip()
        except Exception as e:
            LoggingSystem.log(f"Error sending command: {e}", LogLevel.ERROR)
            return None

    def get_capabilities(self, max_retries: int = 3) -> Optional[str]:
        capabilities_command = "<command>getCapabilities:</command>"
        for attempt in range(max_retries):
            response = self.send_command(capabilities_command)
            LoggingSystem.log(f"Capabilities response attempt {attempt + 1}: {response}", LogLevel.INFO)
            if response:
                self.capabilities = response
                return response
        LoggingSystem.log("Failed to fetch Arduino capabilities after retries.", LogLevel.ERROR)
        return None

class LLMProcessor:
    """Handles all LLM operations with functional approach"""

    def __init__(self, model_path: str, context_size: int = 4096) -> None:
        self.llm = Llama(model_path=model_path, n_ctx=context_size)

    def generate_response(self, prompt_xml: str) -> str:
        response_text = ""
        for response in self.llm.create_completion(
            prompt_xml,
            max_tokens=2048,
            stream=True,
            temperature=0.7,
            repeat_penalty=1.05,
            top_p=0.8,
            top_k=20
        ):
            token = response['choices'][0]['text']
            response_text += token
            if "</response>" in response_text or "</ response>" in response_text:
                break
        return response_text

    def validate_response(self, response_text: str) -> Tuple[Optional[Dict[str, Any]], Optional[Dict[str, Any]]]:
        try:
            response_text = response_text.replace("</ response>", "</response>")
            start_idx = response_text.find('<response>')
            if start_idx == -1:
                LoggingSystem.log(f"Response does not contain <response> tag: {response_text}", LogLevel.ERROR)
                return None, None

            response_text = response_text[start_idx:]
            LoggingSystem.log(f"Cleaned XML response: {response_text}", LogLevel.INFO)

            root = ET.fromstring(response_text)
            chat = root.find('chat').text if root.find('chat') is not None else None

            commands = []
            arduino_node = root.find('arduino')
            if arduino_node is not None:
                commands_node = arduino_node.find('commands')
                if commands_node is not None:
                    commands = [cmd.text for cmd in commands_node.findall('command')]

            state_node = root.find('state')
            state = {
                'current_mood': state_node.find('currentMood').text if state_node is not None and state_node.find('currentMood') is not None else None,
                'current_wonder': state_node.find('whatYouWonderAbout').text if state_node is not None and state_node.find('whatYouWonderAbout') is not None else None,
                'primary_directive': state_node.find('primaryDirective').text if state_node is not None and state_node.find('primaryDirective') is not None else None,
            }

            return {'chat': chat, 'commands': commands}, state
        except ET.ParseError as e:
            LoggingSystem.log(f"XML Parse Error: {e}\nProblematic XML: {response_text}", LogLevel.ERROR)
            return None, None
        except Exception as e:
            LoggingSystem.log(f"Invalid XML response from LLM: {e}", LogLevel.ERROR)
            return None, None

class DialogueEngine:
    """Manages conversation flow and LLM interaction"""

    def __init__(self, llm_processor: LLMProcessor, hardware: HardwareCommunicator, state_manager: StateManager) -> None:
        self.llm_processor = llm_processor
        self.hardware = hardware
        self.state_manager = state_manager
        self.conversation_history = state_manager.current_state.get("conversation_history", [])

    def format_prompt(self, user_command: str, context: Dict[str, Any]) -> str:
        memory_summaries = {
            domain.value: self.state_manager.get_memory_summary(domain)
            for domain in MemoryDomain
        }

        history_str = "\n".join(self.conversation_history[-5:]) if self.conversation_history else "No conversation history yet"

        prompt_xml = f"""
        You are an AI assistant that is both a conversational partner and a tool operator for an Arduino.
        The Arduino to which you are connected has the following capabilities:
        ```
        {context['capabilities']}
        ```
        Your current state:
        ```
        Mood: {context['current_mood']}
        Wonder: {context['current_wonder']}
        Directive: {context['primary_directive']}
        ```

        Memory summaries:
        {json.dumps(memory_summaries, indent=2)}

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

    def process_instruction(self, user_command: str) -> Tuple[Dict[str, Any], Dict[str, Any]]:
        context = {
            'capabilities': self.hardware.capabilities,
            'current_mood': self.state_manager.current_state['current_mood'],
            'current_wonder': self.state_manager.current_state['current_wonder'],
            'primary_directive': self.state_manager.current_state['primary_directive'],
            'last_successful_commands': self.state_manager.current_state['last_successful_commands']
        }

        prompt_xml = self.format_prompt(user_command, context)
        LoggingSystem.log('Initial XML prompt:' + prompt_xml, LogLevel.INFO)

        response_text = self.llm_processor.generate_response(prompt_xml)
        response_text = response_text.replace('\n', '')
        generated_text = response_text.strip()
        LoggingSystem.log("LLM generated initial response:", LogLevel.INFO)
        LoggingSystem.log(generated_text, LogLevel.INFO)

        response_dict, new_state = self.llm_processor.validate_response(generated_text)
        if not response_dict:
            return {'chat': "I couldn't process your request.", 'commands': None}, self.state_manager.current_state

        initial_chat = response_dict.get('chat', '')
        commands = response_dict.get('commands', [])

        if commands:
            LoggingSystem.log(f"Sending {len(commands)} commands to Arduino...", LogLevel.INFO)
            commands_xml = "<commands>\n"
            for cmd in commands:
                commands_xml += f"    <command>{cmd}</command>\n"
            commands_xml += "</commands>"
            arduino_response = self.hardware.send_command(commands_xml)
            LoggingSystem.log(f"Arduino response: {arduino_response}", LogLevel.INFO)

            if arduino_response:
                self.conversation_history.append(f"Arduino: {arduino_response}")
                LoggingSystem.log("Arduino response added to conversation history.", LogLevel.INFO)
                # Store in sensory memory
                self.state_manager.add_memory_entry(
                    MemoryDomain.SENSORY_MEMORY,
                    "Arduino response",
                    {"response": arduino_response, "commands": commands}
                )
            else:
                LoggingSystem.log("Arduino returned no response.", LogLevel.INFO)
                self.conversation_history.append("Arduino: No response received.")
        else:
            LoggingSystem.log("No commands to send to Arduino.", LogLevel.INFO)

        # Update context with new state
        context.update(new_state)

        # Create final prompt with updated context
        final_prompt_xml = self.format_prompt(user_command, context)
        LoggingSystem.log('Final XML prompt (after Arduino response):' + final_prompt_xml, LogLevel.INFO)

        final_response_text = self.llm_processor.generate_response(final_prompt_xml)
        final_response_text = final_response_text.replace('\n', '')
        final_generated_text = final_response_text.strip()
        LoggingSystem.log("LLM generated final response after Arduino feedback:", LogLevel.INFO)
        LoggingSystem.log(final_generated_text, LogLevel.INFO)

        final_response_dict, _ = self.llm_processor.validate_response(final_generated_text)
        final_chat = final_response_dict.get('chat', initial_chat)

        # Store conversation in episodic memory
        self.state_manager.add_memory_entry(
            MemoryDomain.EPISODIC_MEMORY,
            "Conversation",
            {
                "user": user_command,
                "assistant": final_chat,
                "commands": commands,
                "arduino_response": arduino_response
            }
        )

        # Update state with new values
        if new_state:
            self.state_manager.current_state.update(new_state)

        return {
            'chat': final_chat,
            'commands': commands
        }, self.state_manager.current_state

class SpeechProcessor:
    """Handles all speech recognition and synthesis operations"""

    def __init__(self) -> None:
        self.tts_engine = pyttsx3.init()
        self.transcription_model = None
        self.is_recording = False

    def record_audio(self, filename: str = "output.wav", record_seconds: int = 5, sample_rate: int = 16000) -> None:
        chunk = 1024
        format = pyaudio.paInt16
        channels = 1
        p = pyaudio.PyAudio()
        stream = p.open(format=format,
                        channels=channels,
                        rate=sample_rate,
                        input=True,
                        frames_per_buffer=chunk)
        LoggingSystem.log("Recording...", LogLevel.INFO)
        frames = []
        for _ in range(0, int(sample_rate / chunk * record_seconds)):
            data = stream.read(chunk)
            frames.append(data)
        LoggingSystem.log("Finished recording.", LogLevel.INFO)
        stream.stop_stream()
        stream.close()
        p.terminate()
        wf = wave.open(filename, 'wb')
        wf.setnchannels(channels)
        wf.setsampwidth(p.get_sample_size(format))
        wf.setframerate(sample_rate)
        wf.writeframes(b''.join(frames))
        wf.close()

    def transcribe_audio(self, filename: str = "output.wav") -> Optional[str]:
        if not self.transcription_model:
            self.transcription_model = whisper.load_model("./tiny.en.pt")
        result = self.transcription_model.transcribe(filename, fp16=False)
        LoggingSystem.log("You said this: " + str(result), LogLevel.INFO)
        return result["text"] if result else None

    def speak(self, text: str) -> None:
        LoggingSystem.log("I said this: " + text, LogLevel.INFO)
        self.tts_engine.say(text)
        self.tts_engine.runAndWait()

class InputManager:
    """Handles all input processing"""

    def __init__(self, console_mode: bool = False) -> None:
        self.console_mode = console_mode
        self.automation_active = False
        self.record_queue = deque(maxlen=30)

    def add_to_queue(self, user_utterance: str) -> None:
        self.record_queue.append(user_utterance)

    def process_input(self, user_utterance: str, dialogue_engine: DialogueEngine) -> None:
        if user_utterance.strip():
            LoggingSystem.log("Processing user input...", LogLevel.INFO)

            # Process the instruction through the dialogue engine
            response, updated_state = dialogue_engine.process_instruction(user_utterance)

            # Store conversation in short-term memory
            self.state_manager.add_memory_entry(
                MemoryDomain.SHORT_TERM_MEMORY,
                "User interaction",
                {
                    "user": user_utterance,
                    "assistant": response['chat'],
                    "commands": response['commands']
                }
            )

            # Speak the response
            if response['chat']:
                self.speech_processor.speak(response['chat'])

            # Add to conversation history
            conversation_entry = f"User: {user_utterance}\nAssistant: {response['chat']}"
            self.conversation_history.append(conversation_entry)

if __name__ == "__main__":
    # Initialize logging system
    LoggingSystem.setup_logging()

    # Initialize state manager
    state_manager = StateManager()

    # Initialize hardware communicator (use appropriate serial port)
    hardware = HardwareCommunicator("/dev/ttyACM0")  # Change this to your Arduino's serial port

    # Initialize LLM processor
    llm_processor = LLMProcessor("Magistral-Small-2509-Q4_K_M.gguf")

    # Initialize speech processor
    speech_processor = SpeechProcessor()

    # Initialize dialogue engine
    dialogue_engine = DialogueEngine(llm_processor, hardware, state_manager)

    # Initialize input manager
    input_manager = InputManager(console_mode=False)

    try:
        while True:
            # Record audio
            speech_processor.record_audio()

            # Transcribe audio
            transcription = speech_processor.transcribe_audio()
            if transcription:
                LoggingSystem.log(f"Transcription: {transcription}", LogLevel.INFO)

                # Process the input
                input_manager.process_input(transcription, dialogue_engine)

                # Save state periodically
                state_manager.save_state()

            # Small delay to prevent CPU overload
            time.sleep(0.5)

    except KeyboardInterrupt:
        LoggingSystem.log("Application stopped by user.", LogLevel.INFO)
        state_manager.save_state()
