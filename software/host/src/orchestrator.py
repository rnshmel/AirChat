import asyncio
import json
import random
import socket
import serial.tools.list_ports
import serial_asyncio
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse

MOCK_MODE = False
SHOW_TX_ACK = False
MOCK_ORCH_PORT = 9000
MOCK_PCB_PORT = 9001
UDP_IP = "127.0.0.1"
SERIAL_BAUD = 115200

app = FastAPI(title="AirChat V2 Orchestrator")

# Global State
active_websockets = set()
radio_reader = None
radio_writer = None
mock_sock = None
radio_task = None
tx_queue = None
pcb_ack_event = None

async def connect_radio():
    """Connects to either the UDP Mock PCB or the physical CP2102N over Serial."""
    global radio_reader, radio_writer, mock_sock, radio_task, MOCK_MODE

    # Clean up existing connections
    if mock_sock:
        mock_sock.close()
        mock_sock = None
    if radio_writer:
        radio_writer.close()
        await radio_writer.wait_closed()
        radio_writer = radio_reader = None
        
    if radio_task:
        radio_task.cancel()

    if MOCK_MODE:
        try:
            mock_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            mock_sock.bind((UDP_IP, MOCK_ORCH_PORT))
            mock_sock.setblocking(False)
            radio_task = asyncio.create_task(radio_read_loop())
            return True, f"Connected to Mock PCB via UDP (Port {MOCK_ORCH_PORT})"
        except Exception as e:
            return False, f"Mock UDP Binding Failed: {e}"
    else:
        # Scan for Silicon Labs CP2102N (VID: 10C4, PID: EA60)
        port = None
        for p in serial.tools.list_ports.comports():
            if p.vid == 0x10C4 and p.pid == 0xEA60:
                port = p.device
                break
        
        if not port:
            return False, "CP2102N radio not found. Please check USB connection."
        
        try:
            radio_reader, radio_writer = await serial_asyncio.open_serial_connection(url=port, baudrate=SERIAL_BAUD)
            radio_task = asyncio.create_task(radio_read_loop())
            return True, f"Hardware Radio Connected: {port} @ {SERIAL_BAUD} baud"
        except Exception as e:
            return False, f"Serial Error on {port}: {e}"

async def write_radio(data: bytes):
    """Writes raw bytes out to the active transport."""
    global radio_writer, mock_sock
    if MOCK_MODE and mock_sock:
        try:
            mock_sock.sendto(data, (UDP_IP, MOCK_PCB_PORT))
        except BlockingIOError:
            pass 
    elif radio_writer:
        radio_writer.write(data)
        await radio_writer.drain()

async def radio_write_queue_loop():
    """Background task: Pulls packets from the queue and waits for PCB ACK before sending the next."""
    global tx_queue, pcb_ack_event
    while True:
        packet = await tx_queue.get()
        pcb_ack_event.clear() # Lock the queue
        await write_radio(packet)
        
        try:
            # Wait up to 5 seconds for the PCB to respond with a 0x03 packet
            await asyncio.wait_for(pcb_ack_event.wait(), timeout=5.0)
        except asyncio.TimeoutError:
            print("[WARN] PCB ACK Timeout. Unlocking queue.")
            pcb_ack_event.set()
            await broadcast_ws({"event": "sys_msg", "level": "warning", "message": "Warning: PCB did not acknowledge last command. Queue resumed."})

async def radio_read_loop():
    """Background task to read raw bytes with software IDLE line framing."""
    global radio_reader, mock_sock
    loop = asyncio.get_running_loop()
    rx_buffer = bytearray()
    
    while True:
        try:
            if not rx_buffer:

                chunk = b''
                if MOCK_MODE and mock_sock:
                    chunk = await loop.sock_recv(mock_sock, 1024)
                elif radio_reader:
                    chunk = await radio_reader.read(1024)
                
                if chunk:
                    rx_buffer.extend(chunk)
                else:
                    await asyncio.sleep(0.01)
            else:
 
                try:
                    chunk = b''
                    if MOCK_MODE and mock_sock:
                        chunk = await asyncio.wait_for(loop.sock_recv(mock_sock, 1024), timeout=0.015) # 15ms timeout
                    elif radio_reader:
                        chunk = await asyncio.wait_for(radio_reader.read(1024), timeout=0.015) # 15ms timeout
                        
                    if chunk:
                        rx_buffer.extend(chunk)
                except asyncio.TimeoutError:
                    # 3. 5ms of silence detected. The USB stream is fully assembled!
                    await process_radio_packet(bytes(rx_buffer))
                    rx_buffer.clear()
                    
        except asyncio.CancelledError:
            break
        except Exception as e:
            print(f"[ERR] Radio read loop exception: {e}")
            await asyncio.sleep(1)

async def process_radio_packet(data: bytes):
    """Decodes Domain B serial packets and routes them to the Web UI."""
    if not data: return
    packet_id = data[0]
    
    # 0x02: RX Message (Bidirectional OTA wrapper)
    if packet_id == 0x02 and len(data) >= 3:
        color_id = data[1]
        name_len = data[2]
        
        if len(data) >= 3 + name_len:
            username = data[3:3+name_len].decode('ascii', errors='replace')
            msg_text = data[3+name_len:].decode('ascii', errors='replace')
            
            await broadcast_ws({
                "event": "rx_msg",
                "username": username,
                "color_id": color_id,
                "message": msg_text
            })
            
    # 0x03: System Status (PCB to Orchestrator)
    elif packet_id == 0x03 and len(data) >= 2:
        status_code = data[1]
        level = "info"
        
        if pcb_ack_event:
            pcb_ack_event.set()
        
        if status_code == 0x00:
            msg = "Hardware Reset Detected. Configuration lost, please re-apply config."
            level = "warning"
        elif status_code == 0x01:
            if not SHOW_TX_ACK:
                return 
            msg = "TX Complete"
        elif status_code == 0x02:
            msg = "CCA Timeout / Channel Busy (TX Failed)"
            level = "warning"
        elif status_code == 0x03: msg = "PCB Configuration Received and Loaded"
        elif status_code == 0xFF:
            msg = "CC1101 Hardware Error Detected!"
            level = "error"
        else:
            msg = f"Unknown PCB Status Code: {hex(status_code)}"
            
        await broadcast_ws({
            "event": "sys_msg",
            "level": level,
            "message": msg
        })

async def broadcast_ws(payload: dict):
    """Sends JSON to all connected browser tabs."""
    dead_sockets = set()
    for ws in active_websockets:
        try:
            await ws.send_json(payload)
        except Exception:
            dead_sockets.add(ws)
    active_websockets.difference_update(dead_sockets)

@app.on_event("startup")
async def startup_event():
    """Initializes globals and the mock radio connection on boot."""
    global tx_queue, pcb_ack_event
    tx_queue = asyncio.Queue()
    pcb_ack_event = asyncio.Event()
    pcb_ack_event.set() 
    
    asyncio.create_task(radio_write_queue_loop())
    
    success, msg = await connect_radio()
    print(f"[SYS] {msg}")

@app.get("/")
async def serve_ui():
    return FileResponse("index.html")

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    global MOCK_MODE
    await websocket.accept()
    active_websockets.add(websocket)
    
    mode_str = "UDP MOCK MODE" if MOCK_MODE else "HARDWARE SERIAL MODE"
    await websocket.send_json({"event": "sys_msg", "level": "info", "message": f"Orchestrator Backend Linked. [{mode_str}]"})
    
    try:
        while True:
            raw_data = await websocket.receive_text()
            payload = json.loads(raw_data)
            command = payload.get("command")
            
            if command == "scan_radio":
                MOCK_MODE = False
                success, msg = await connect_radio()
                await broadcast_ws({"event": "sys_msg", "level": "info" if success else "error", "message": msg})
                
            elif command == "set_config":
                if not (MOCK_MODE and mock_sock) and not radio_writer:
                    await websocket.send_json({"event": "sys_msg", "level": "error", "message": "Config Failed: No radio connected. Please scan for hardware."})
                    continue
                    
                channel = int(payload.get("channel", 0)) & 0x0F
                mode = int(payload.get("mode", 1)) & 0x03
                seed = random.randint(0, 255)
                
                packet = bytes([0x01, channel, mode, seed])
                await tx_queue.put(packet)
                
            elif command == "tx_msg":
                if not (MOCK_MODE and mock_sock) and not radio_writer:
                    await websocket.send_json({"event": "sys_msg", "level": "error", "message": "TX Failed: No radio connected. Please scan for hardware."})
                    continue
                    
                name_bytes = payload.get("username", "guest").encode('ascii', 'replace')[:16] 
                msg_bytes = payload.get("message", "").encode('ascii', 'replace')[:224] 
                
                packet = bytes([0x02, int(payload.get("color_id", 2)) & 0x0F, len(name_bytes)]) + name_bytes + msg_bytes
                await tx_queue.put(packet)
                
    except WebSocketDisconnect:
        active_websockets.remove(websocket)
