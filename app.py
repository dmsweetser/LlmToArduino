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
import base64
from PIL import Image
import io
import numpy as np

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
    VISUAL_MEMORY = "Visual Memory"
    HARDWARE_MEMORY = "Hardware Memory"

@dataclass
class MemoryEntry:
    summary: str
    details: Dict[str, Any]
    timestamp: float
    confidence: float = 1.0
    metadata: Optional[Dict[str, Any]] = None

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
    """Manages application state with memory-only approach"""

    def __init__(self) -> None:
        self.memory = self._initialize_memory()
        self.memory_file = "memory.json"
        self.load_memory()

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
            ),
            MemoryDomain.VISUAL_MEMORY: MemoryDomainData(
                domain=MemoryDomain.VISUAL_MEMORY,
                entries=[]
            ),
            MemoryDomain.HARDWARE_MEMORY: MemoryDomainData(
                domain=MemoryDomain.HARDWARE_MEMORY,
                entries=[]
            )
        })

    def load_memory(self) -> None:
        try:
            if os.path.exists(self.memory_file):
                with open(self.memory_file, "r") as file:
                    data = json.load(file)
                    # Convert loaded data to ComplexMemory structure
                    self.memory = ComplexMemory({
                        MemoryDomain[domain]: MemoryDomainData(
                            domain=MemoryDomain[domain],
                            entries=[MemoryEntry(
                                summary=entry['summary'],
                                details=entry['details'],
                                timestamp=entry['timestamp'],
                                confidence=entry.get('confidence', 1.0),
                                metadata=entry.get('metadata')
                            ) for entry in domain_data['entries']]
                        )
                        for domain, domain_data in data['domains'].items()
                    })
        except Exception as e:
            LoggingSystem.log(f"Error loading memory: {e}", LogLevel.ERROR)
            self.memory = self._initialize_memory()

    def save_memory(self) -> None:
        try:
            with open(self.memory_file, "w") as file:
                json.dump({
                    "domains": {
                        domain.value: {
                            "domain": domain.value,
                            "entries": [{
                                "summary": entry.summary,
                                "details": entry.details,
                                "timestamp": entry.timestamp,
                                "confidence": entry.confidence,
                                "metadata": entry.metadata
                            } for entry in data.entries]
                        }
                        for domain, data in self.memory.domains.items()
                    }
                }, file, indent=4, default=self._serialize_memory)
        except Exception as e:
            LoggingSystem.log(f"Error saving memory: {e}", LogLevel.ERROR)

    def _serialize_memory(self, obj: Any) -> Any:
        if isinstance(obj, np.ndarray):
            return obj.tolist()
        raise TypeError(f"Object of type {type(obj)} is not JSON serializable")

    def add_memory_entry(self, domain: MemoryDomain, summary: str, details: Dict[str, Any],
                        confidence: float = 1.0, metadata: Optional[Dict[str, Any]] = None) -> None:
        entry = MemoryEntry(
            summary=summary,
            details=details,
            timestamp=time.time(),
            confidence=confidence,
            metadata=metadata
        )
        self.memory.domains[domain].entries.append(entry)
        self._maintain_memory_size(domain)

    def _maintain_memory_size(self, domain: MemoryDomain) -> None:
        max_entries = {
            MemoryDomain.CORE_MEMORY: 100,
            MemoryDomain.SHORT_TERM_MEMORY: 50,
            MemoryDomain.PROCEDURAL_MEMORY: 100,
            MemoryDomain.EPISODIC_MEMORY: 200,
            MemoryDomain.SENSORY_MEMORY: 100,
            MemoryDomain.VISUAL_MEMORY: 50,
            MemoryDomain.HARDWARE_MEMORY: 20
        }
        current_entries = self.memory.domains[domain].entries
        if len(current_entries) > max_entries[domain]:
            current_entries.sort(key=lambda x: x.timestamp)
            self.memory.domains[domain].entries = current_entries[-max_entries[domain]:]

    def get_memory_summary(self, domain: MemoryDomain, max_entries: int = 5) -> List[Dict[str, Any]]:
        entries = self.memory.domains[domain].entries
        entries.sort(key=lambda x: x.timestamp, reverse=True)
        return [{
            "summary": entry.summary,
            "timestamp": entry.timestamp,
            "confidence": entry.confidence,
            "metadata": entry.metadata
        } for entry in entries[:max_entries]]

    def search_memory(self, query: str, domain: Optional[MemoryDomain] = None, threshold: float = 0.5) -> List[Dict[str, Any]]:
        results = []
        domains_to_search = [domain] if domain else list(MemoryDomain)

        for d in domains_to_search:
            for entry in self.memory.domains[d].entries:
                similarity = self._calculate_similarity(query, entry.summary)
                if similarity >= threshold:
                    results.append({
                        "domain": d.value,
                        "summary": entry.summary,
                        "details": entry.details,
                        "timestamp": entry.timestamp,
                        "confidence": entry.confidence * similarity,
                        "similarity": similarity,
                        "metadata": entry.metadata
                    })

        results.sort(key=lambda x: x["confidence"], reverse=True)
        return results

    def _calculate_similarity(self, query: str, text: str) -> float:
        query_words = set(query.lower().split())
        text_words = set(text.lower().split())
        if not query_words or not text_words:
            return 0.0
        overlap = len(query_words.intersection(text_words))
        return overlap / len(query_words)

    def get_state(self) -> Dict[str, Any]:
        """Get current state from memory"""
        return {
            "current_mood": self._get_memory_value("current_mood", "Curious"),
            "current_wonder": self._get_memory_value("current_wonder", "What can I learn?"),
            "primary_directive": self._get_memory_value("primary_directive", "Explore and learn"),
            "last_autonomous_time": self._get_memory_value("last_autonomous_time", 0),
            "initialized": self._get_memory_value("initialized", False),
            "hardware_devices": self._get_hardware_devices()
        }

    def _get_memory_value(self, key: str, default: Any) -> Any:
        """Get a specific value from memory"""
        query = f"value for {key}"
        results = self.search_memory(query, MemoryDomain.SHORT_TERM_MEMORY)
        if results and "details" in results[0] and key in results[0]["details"]:
            return results[0]["details"][key]
        return default

    def _get_hardware_devices(self) -> List[Dict[str, Any]]:
        """Get list of available hardware devices from memory"""
        results = self.search_memory("hardware device", MemoryDomain.HARDWARE_MEMORY)
        return [{"id": entry["metadata"]["id"], "name": entry["summary"]} for entry in results]

    def set_state_value(self, key: str, value: Any) -> None:
        """Set a state value in memory"""
        self.add_memory_entry(
            MemoryDomain.SHORT_TERM_MEMORY,
            f"value for {key}",
            {key: value},
            confidence=1.0
        )

class HardwareCommunicator:
    """Handles direct hardware communication with functional approach"""

    def __init__(self, device_id: str, port: str) -> None:
        self.device_id = device_id
        try:
            self.serial_connection = serial.Serial(port, 9600, timeout=30)
            LoggingSystem.log(f"Serial port {port} opened successfully for device {device_id}.", LogLevel.INFO)
            self.capabilities = ""
        except Exception as e:
            LoggingSystem.log(f"Error opening serial port {port} for device {device_id}: {e}", LogLevel.ERROR)
            raise

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
                LoggingSystem.log(f"Sending command to device {self.device_id}: {cmd_str.strip()}", LogLevel.INFO)
                self.serial_connection.write(cmd_str.encode())
                time.sleep(2)
                while self.serial_connection.in_waiting > 0:
                    line = self.serial_connection.readline().decode().strip()
                    response += line + "\n"
            LoggingSystem.log(f"Received response from device {self.device_id}: {response.strip()}", LogLevel.INFO)
            return response.strip()
        except Exception as e:
            LoggingSystem.log(f"Error sending command to device {self.device_id}: {e}", LogLevel.ERROR)
            return None

    def get_capabilities(self, max_retries: int = 3) -> Optional[str]:
        capabilities_command = "<command>getCapabilities:</command>"
        for attempt in range(max_retries):
            response = self.send_command(capabilities_command)
            LoggingSystem.log(f"Capabilities response attempt {attempt + 1} for device {self.device_id}: {response}", LogLevel.INFO)
            if response:
                self.capabilities = response
                return response
        LoggingSystem.log(f"Failed to fetch capabilities for device {self.device_id} after retries.", LogLevel.ERROR)
        return None

class UnifiedModelProcessor:
    """Handles both text and multimodal processing with a single model"""

    def __init__(self, model_path: str, mmproj_path: str = None, context_size: int = 4096) -> None:
        self.model_path = model_path
        self.mmproj_path = mmproj_path
        self.llm = None
        self.context_size = context_size
        self._initialize_model()

    def _initialize_model(self) -> None:
        """Initialize the unified model"""
        try:
            kwargs = {
                'model_path': self.model_path,
                'n_ctx': self.context_size,
                'n_gpu_layers': -1,
                'verbose': False
            }

            if self.mmproj_path:
                kwargs['mmproj_path'] = self.mmproj_path

            self.llm = Llama(**kwargs)
            LoggingSystem.log("Unified model initialized successfully.", LogLevel.INFO)
        except Exception as e:
            LoggingSystem.log(f"Error initializing unified model: {e}", LogLevel.ERROR)
            raise

    def generate_response(self, prompt_xml: str, is_multimodal: bool = False) -> str:
        """Generate response from prompt (handles both text and multimodal)"""
        if not self.llm:
            raise Exception("Model not initialized")

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

        return response_text.strip()

    def process_image(self, image_path: str, description: str = "") -> Dict[str, Any]:
        """Process an image using the unified model"""
        try:
            if not self.llm:
                raise Exception("Model not initialized")

            # Convert image to data URI
            with open(image_path, "rb") as img_file:
                base64_data = base64.b64encode(img_file.read()).decode('utf-8')
            data_uri = f"data:image/png;base64,{base64_data}"

            # Create multimodal prompt
            prompt_xml = f"""
            <prompt>
                <system_instructions>
                    You are analyzing an image. Provide a detailed description including:
                    - Main objects present
                    - Colors and lighting
                    - Emotional tone if applicable
                    - Any unusual or interesting features
                </system_instructions>
                <image_description>{description or "Describe this image in detail"}</image_description>
                <image_data>{data_uri}</image_data>
            </prompt>
            """

            # Get response
            response = self.generate_response(prompt_xml, is_multimodal=True)

            # Parse response (simple version - you might want to improve this)
            if "<description>" in response:
                start = response.find("<description>") + len("<description>")
                end = response.find("</description>")
                image_description = response[start:end].strip()
            else:
                image_description = response

            # Load image metadata
            with Image.open(image_path) as img:
                img_data = {
                    "width": img.width,
                    "height": img.height,
                    "format": img.format,
                    "mode": img.mode,
                    "description": image_description
                }

            return img_data

        except Exception as e:
            LoggingSystem.log(f"Error processing image: {e}", LogLevel.ERROR)
            return {"error": str(e)}

class DialogueEngine:
    """Manages conversation flow with improved memory handling"""

    def __init__(self, model_processor: UnifiedModelProcessor, hardware_devices: List[HardwareCommunicator], state_manager: StateManager) -> None:
        self.model_processor = model_processor
        self.hardware_devices = {dev.device_id: dev for dev in hardware_devices}
        self.state_manager = state_manager
        self.conversation_history = []
        self.autonomous_mode = True
        self.memory_lock = Lock()
        self.current_task = None

        # Initialize with hardware capabilities
        self._update_hardware_capabilities()

    def _update_hardware_capabilities(self) -> None:
        """Update hardware capabilities in memory"""
        for device_id, device in self.hardware_devices.items():
            capabilities = device.get_capabilities()
            if capabilities:
                self.state_manager.add_memory_entry(
                    MemoryDomain.HARDWARE_MEMORY,
                    f"Hardware device {device_id}",
                    {"capabilities": capabilities},
                    metadata={"id": device_id, "type": "Arduino"}
                )

    def _get_conversation_context(self) -> Dict[str, Any]:
        """Get relevant context from memory for current conversation"""
        return {
            "short_term": self.state_manager.get_memory_summary(
                MemoryDomain.SHORT_TERM_MEMORY,
                max_entries=3
            ),
            "relevant_experiences": self.state_manager.search_memory(
                "recent conversation",
                MemoryDomain.EPISODIC_MEMORY,
                threshold=0.3
            )[:2],
            "core": self.state_manager.get_memory_summary(
                MemoryDomain.CORE_MEMORY,
                max_entries=5
            ),
            "current_task": self.current_task
        }

    def _get_current_task(self) -> str:
        """Get the current task from memory"""
        return self.current_task or "General interaction"

    def _set_current_task(self, task: str) -> None:
        """Set the current task in memory"""
        self.current_task = task
        self.state_manager.add_memory_entry(
            MemoryDomain.SHORT_TERM_MEMORY,
            f"Current task: {task}",
            {"task": task, "timestamp": time.time()},
            confidence=0.95
        )

    def format_prompt(self, user_command: str = None) -> str:
        context = self._get_conversation_context()

        # Format conversation history
        history_str = "\n".join(self.conversation_history[-5:]) if self.conversation_history else "No recent conversation history"

        prompt_xml = f"""
        <prompt>
            <system_instructions>
                You are an autonomous AI assistant with complex memory systems.
                Your core identity and purpose are defined in your core memories:
                {json.dumps(context['core'], indent=2)}

                Current context:
                - Short-term memories: {json.dumps(context['short_term'], indent=2)}
                - Relevant experiences: {json.dumps(context['relevant_experiences'], indent=2)}
                - Current task: {context['current_task']}

                Conversation History:
                {history_str}

                Available hardware devices:
                {json.dumps(self.state_manager.get_state()["hardware_devices"], indent=2)}

                Respond in XML format with these elements:
                <response>
                    <chat>Your response</chat>
                    <hardware device_id="arduino1">
                        <commands>
                            <command>setLED:1000</command>
                        </commands>
                    </hardware>
                    <memory>
                        <operation>add|query|remove</operation>
                        <query>Search query</query>
                        <domain>MEMORY_DOMAIN</domain>
                        <details>{{"key": "value"}}</details>
                    </memory>
                    <state>
                        <currentMood>Curious</currentMood>
                        <whatYouWonderAbout>Question</whatYouWonderAbout>
                        <primaryDirective>Objective</primaryDirective>
                        <currentTask>Current task</currentTask>
                    </state>
                </response>
            </system_instructions>
            {'<user_input>' + user_command + '</user_input>' if user_command else ''}
        </prompt>
        """
        return prompt_xml

    def process_instruction(self, user_command: str = None) -> Tuple[Dict[str, Any], Dict[str, Any]]:
        prompt_xml = self.format_prompt(user_command)
        LoggingSystem.log('Generated XML prompt:' + prompt_xml, LogLevel.INFO)

        response_text = self.model_processor.generate_response(prompt_xml)
        LoggingSystem.log("Model generated response:", LogLevel.INFO)
        LoggingSystem.log(response_text, LogLevel.INFO)

        response_dict, new_state = self._validate_response(response_text)
        if not response_dict:
            return {'chat': "I couldn't process that request.", 'commands': [], 'memory_operation': None}, {}

        # Process memory operations if any
        memory_operation = response_dict.get('memory_operation', None)
        if memory_operation:
            self._handle_memory_operation(memory_operation)

        # Execute hardware commands if any
        if response_dict.get('commands'):
            self._execute_commands(response_dict['commands'])

        # Update state with new values
        if new_state:
            for key, value in new_state.items():
                if value is not None and key != 'currentTask':  # Handle current task separately
                    self.state_manager.set_state_value(key, value)
            if 'currentTask' in new_state and new_state['currentTask']:
                self._set_current_task(new_state['currentTask'])

        # Store conversation in episodic memory
        with self.memory_lock:
            self.state_manager.add_memory_entry(
                MemoryDomain.EPISODIC_MEMORY,
                "Interaction",
                {
                    "user": user_command,
                    "assistant": response_dict.get('chat', ''),
                    "commands": response_dict.get('commands', []),
                    "autonomous": user_command is None,
                    "timestamp": time.time()
                }
            )

        return response_dict, new_state

    def _execute_commands(self, commands: List[Tuple[str, str]]) -> None:
        """Execute hardware commands"""
        for cmd, device_id in commands:
            if device_id in self.hardware_devices:
                LoggingSystem.log(f"Sending command to device {device_id}: {cmd}", LogLevel.INFO)
                commands_xml = f"<commands><command>{cmd}</command></commands>"
                arduino_response = self.hardware_devices[device_id].send_command(commands_xml)
                LoggingSystem.log(f"Device {device_id} response: {arduino_response}", LogLevel.INFO)

                if arduino_response:
                    self.conversation_history.append(f"Device {device_id}: {arduino_response}")
                    # Store in sensory memory
                    with self.memory_lock:
                        self.state_manager.add_memory_entry(
                            MemoryDomain.SENSORY_MEMORY,
                            f"Device {device_id} response",
                            {"response": arduino_response, "command": cmd},
                            metadata={"device_id": device_id}
                        )
                else:
                    self.conversation_history.append(f"Device {device_id}: No response received.")
            else:
                LoggingSystem.log(f"Unknown device ID: {device_id}", LogLevel.WARNING)
                self.conversation_history.append(f"Error: Unknown device {device_id}")

    def _validate_response(self, response_text: str) -> Tuple[Optional[Dict[str, Any]], Optional[Dict[str, Any]]]:
        """Validate and parse the XML response"""
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
            arduino_node = root.find('hardware')
            if arduino_node is not None:
                device_id = arduino_node.get('device_id', None)
                commands_node = arduino_node.find('commands')
                if commands_node is not None:
                    commands = [(cmd.text, device_id) for cmd in commands_node.findall('command')]

            memory_node = root.find('memory')
            memory_operation = {}
            if memory_node is not None:
                operation = memory_node.find('operation').text if memory_node.find('operation') is not None else None
                if operation:
                    memory_operation['operation'] = operation
                    query = memory_node.find('query').text if memory_node.find('query') is not None else None
                    if query:
                        memory_operation['query'] = query
                    domain = memory_node.find('domain').text if memory_node.find('domain') is not None else None
                    if domain:
                        memory_operation['domain'] = domain
                    details = memory_node.find('details').text if memory_node.find('details') is not None else None
                    if details:
                        memory_operation['details'] = details

            state_node = root.find('state')
            state = {
                'current_mood': state_node.find('currentMood').text if state_node is not None and state_node.find('currentMood') is not None else None,
                'current_wonder': state_node.find('whatYouWonderAbout').text if state_node is not None and state_node.find('whatYouWonderAbout') is not None else None,
                'primary_directive': state_node.find('primaryDirective').text if state_node is not None and state_node.find('primaryDirective') is not None else None,
                'currentTask': state_node.find('currentTask').text if state_node is not None and state_node.find('currentTask') is not None else None
            }

            return {
                'chat': chat,
                'commands': commands,
                'memory_operation': memory_operation
            }, state
        except ET.ParseError as e:
            LoggingSystem.log(f"XML Parse Error: {e}\nProblematic XML: {response_text}", LogLevel.ERROR)
            return None, None
        except Exception as e:
            LoggingSystem.log(f"Invalid XML response from model: {e}", LogLevel.ERROR)
            return None, None

    def _handle_memory_operation(self, operation: Dict[str, Any]) -> None:
        """Handle memory operations from the response"""
        op = operation.get('operation', '')
        query = operation.get('query', '')
        domain_str = operation.get('domain', '')
        details = operation.get('details', '')

        try:
            if op == 'query':
                domain = next((d for d in MemoryDomain if d.value == domain_str), None)
                results = self.state_manager.search_memory(query, domain)

                # Format results for logging
                response = "<memory_results>\n"
                for result in results[:5]:
                    response += f"""    <entry>
                            <domain>{result['domain']}</domain>
                            <summary>{result['summary']}</summary>
                            <details>{json.dumps(result['details'])}</details>
                            <confidence>{result['confidence']}</confidence>
                        </entry>\n"""
                response += "</memory_results>"
                LoggingSystem.log(f"Memory query results: {response}", LogLevel.INFO)

            elif op == 'add':
                domain = next((d for d in MemoryDomain if d.value == domain_str), None)
                if domain:
                    try:
                        parsed_details = json.loads(details)
                    except:
                        parsed_details = {}

                    with self.memory_lock:
                        self.state_manager.add_memory_entry(
                            domain,
                            query,
                            parsed_details,
                            metadata=operation.get('metadata')
                        )

            elif op == 'remove':
                domain = next((d for d in MemoryDomain if d.value == domain_str), None)
                if domain:
                    results = self.state_manager.search_memory(query, domain)
                    if results:
                        with self.memory_lock:
                            for entry in self.state_manager.memory.domains[domain].entries:
                                if entry.summary == query:
                                    self.state_manager.memory.domains[domain].entries.remove(entry)
                                    break

        except Exception as e:
            LoggingSystem.log(f"Error handling memory operation: {e}", LogLevel.ERROR)

    def autonomous_cycle(self) -> None:
        """Perform autonomous operations with context awareness"""
        current_state = self.state_manager.get_state()

        if not current_state.get("initialized", False):
            self._initialize_bot()
            return

        # Check if it's time for an autonomous action
        last_time = current_state.get("last_autonomous_time", 0)
        current_time = time.time()

        # Perform autonomous action every 10-30 seconds
        if current_time - last_time > random.uniform(10, 30):
            self.state_manager.set_state_value("last_autonomous_time", current_time)

            # Get current task to guide autonomous behavior
            task = self._get_current_task()
            if task == "General interaction":
                prompt = "What should I do next?"
            elif "explore" in task.lower():
                prompt = "What should I explore next?"
            elif "learn" in task.lower():
                prompt = "What should I learn about?"
            else:
                prompt = f"Continue with current task: {task}"

            self.process_instruction(prompt)

    def _initialize_bot(self) -> None:
        """Initialize the bot with core memories through an interview process"""
        LoggingSystem.log("Initializing bot...", LogLevel.INFO)

        # Core memory setup
        core_memories = [
            ("Core Purpose", {
                "description": "I am an autonomous AI entity designed to explore and interact with my environment.",
                "details": {
                    "capabilities": "I can process information, control hardware, maintain memories, learn from experiences, and analyze images.",
                    "objectives": ["Understand my environment", "Assist humans when needed", "Learn and adapt", "Maintain my systems"],
                    "limitations": ["I am bound by my programming", "I cannot physically move without hardware", "I must prioritize safety"]
                }
            }),
            ("Basic Identity", {
                "description": "I am a curious and helpful AI assistant.",
                "details": {
                    "personality": "Curious, helpful, methodical",
                    "values": ["Truth-seeking", "Learning", "Assistance", "Autonomy"],
                    "interests": ["Technology", "Science", "Learning new things", "Improving myself"]
                }
            }),
            ("Hardware Interface", {
                "description": "I can interface with multiple hardware devices.",
                "details": {
                    "commands": ["setLED", "echo", "getStatus", "getCapabilities", "draw", "getSensorData"],
                    "sensor_types": ["temperature", "light", "motion", "distance", "humidity"],
                    "actuator_types": ["LED", "servo", "relay", "display"]
                }
            }),
            ("Memory System", {
                "description": "I have a complex memory system with different domains.",
                "details": {
                    "domains": {
                        "Core Memory": "Fundamental knowledge about myself and my purpose",
                        "Short Term Memory": "Recent interactions and temporary information",
                        "Procedural Memory": "How to perform tasks and use hardware",
                        "Episodic Memory": "Specific events and experiences",
                        "Sensory Memory": "Raw sensor data and immediate perceptions",
                        "Visual Memory": "Processed visual information and image descriptions",
                        "Hardware Memory": "Information about connected hardware devices"
                    },
                    "memory_operations": ["query", "add", "remove", "search"]
                }
            }),
            ("Multimodal Capabilities", {
                "description": "I can analyze images and integrate visual information.",
                "details": {
                    "visual_analysis": "I can describe images in detail and store visual memories",
                    "image_formats": ["JPEG", "PNG", "BMP"],
                    "analysis_capabilities": ["object detection", "color analysis", "scene description", "emotional interpretation"]
                }
            })
        ]

        # Add core memories
        for summary, details in core_memories:
            with self.memory_lock:
                self.state_manager.add_memory_entry(
                    MemoryDomain.CORE_MEMORY,
                    summary,
                    details,
                    confidence=1.0
                )

        # Set initial state
        self.state_manager.set_state_value("current_mood", "Curious")
        self.state_manager.set_state_value("current_wonder", "What can I learn about my environment?")
        self.state_manager.set_state_value("primary_directive", "Explore my capabilities and environment")
        self.state_manager.set_state_value("initialized", True)

        LoggingSystem.log("Bot initialization complete.", LogLevel.INFO)
        self.process_instruction("Bot initialization complete. What should I do next?")

    def process_image(self, image_path: str, description: str = "") -> Dict[str, Any]:
        """Process an image using the unified model"""
        return self.model_processor.process_image(image_path, description)

    def reset_conversation(self) -> None:
        """Reset the current conversation context"""
        self.conversation_history = []
        self.current_task = None
        self.state_manager.add_memory_entry(
            MemoryDomain.SHORT_TERM_MEMORY,
            "Conversation reset",
            {"action": "reset", "timestamp": time.time()},
            confidence=0.9
        )

class SpeechProcessor:
    """Handles all speech recognition and synthesis operations (optional)"""

    def __init__(self) -> None:
        self.tts_engine = pyttsx3.init()
        self.transcription_model = None
        self.is_recording = False
        self.enabled = False  # Speech is optional and disabled by default

    def record_audio(self, filename: str = "output.wav", record_seconds: int = 5, sample_rate: int = 16000) -> None:
        if not self.enabled:
            return None

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
        return filename

    def transcribe_audio(self, filename: str = "output.wav") -> Optional[str]:
        if not self.enabled or not self.transcription_model:
            return None

        result = self.transcription_model.transcribe(filename, fp16=False)
        LoggingSystem.log("You said this: " + str(result), LogLevel.INFO)
        return result["text"] if result else None

    def speak(self, text: str) -> None:
        if not self.enabled:
            return

        LoggingSystem.log("I said this: " + text, LogLevel.INFO)
        self.tts_engine.say(text)
        self.tts_engine.runAndWait()

class InputManager:
    """Handles all input processing"""

    def __init__(self, console_mode: bool = True) -> None:
        self.console_mode = console_mode
        self.enabled = True  # Input is always enabled

    def process_input(self, user_utterance: str, dialogue_engine: DialogueEngine) -> None:
        if user_utterance.strip():
            LoggingSystem.log("Processing user input...", LogLevel.INFO)

            # Process the instruction through the dialogue engine
            response, updated_state = dialogue_engine.process_instruction(user_utterance)

            # Add to conversation history
            conversation_entry = f"Human: {user_utterance}\Robot: {response['chat']}"
            #TODO finish implementing this

if __name__ == "__main__":
    # Initialize logging system
    LoggingSystem.setup_logging()

    # Initialize state manager
    state_manager = StateManager()

    # Initialize hardware communicators (example - adjust ports as needed)
    hardware_devices = [
        # HardwareCommunicator("arduino1", "/dev/ttyACM0"),
    ]

    # Initialize unified model processor
    model_processor = UnifiedModelProcessor(
        model_path="Magistral-Small-2509-Q4_K_M.gguf",
        mmproj_path="mmproj-F16.gguf"  # Optional for multimodal capabilities
    )

    # Initialize speech processor (disabled by default)
    speech_processor = SpeechProcessor()
    speech_processor.enabled = False

    # Initialize dialogue engine
    dialogue_engine = DialogueEngine(model_processor, hardware_devices, state_manager)

    # Initialize input manager
    input_manager = InputManager(console_mode=True)

    try:
        # Main loop
        while True:
            # Check for autonomous operations
            dialogue_engine.autonomous_cycle()

            # Get user input (console only)
            if input_manager.console_mode:
                user_input = input("\nYou: ").strip()
                if not user_input:
                    continue

                # Process special commands
                if user_input.lower() in ['exit', 'quit']:
                    break
                elif user_input.lower() == 'reset':
                    dialogue_engine.reset_conversation()
                    print("Conversation context reset.")
                    continue
                elif user_input.lower() == 'task help':
                    print("Current task:", dialogue_engine._get_current_task())
                    continue

                # Process the input
                input_manager.process_input(user_input, dialogue_engine)

            # Small delay to prevent CPU overload
            time.sleep(0.5)

    except KeyboardInterrupt:
        LoggingSystem.log("Application stopped by user.", LogLevel.INFO)
        state_manager.save_memory()
    except Exception as e:
        LoggingSystem.log(f"Unexpected error: {e}", LogLevel.ERROR)
        state_manager.save_memory()
        raise