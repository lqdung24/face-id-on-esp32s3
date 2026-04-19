"""
ESP32 Face ID WebSocket Server
Handle real-time JPEG streaming from ESP32
"""

import os
import json
import base64
from datetime import datetime
from pathlib import Path
from flask import Flask, render_template_string, request
from flask_sock import Sock
import logging

# Setup logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = Flask(__name__)
sock = Sock(app)

# Store connected clients
clients = {}
stream_enabled = False

# Create uploads directory
UPLOAD_DIR = Path("uploads")
UPLOAD_DIR.mkdir(exist_ok=True)

DASHBOARD_HTML = """
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 Face ID Dashboard</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 20px;
    }
    .container {
      background: white;
      border-radius: 20px;
      box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
      max-width: 1200px;
      width: 100%;
      padding: 40px;
    }
    header {
      text-align: center;
      margin-bottom: 30px;
    }
    h1 {
      color: #333;
      margin-bottom: 10px;
    }
    .status {
      display: inline-block;
      padding: 8px 16px;
      border-radius: 20px;
      font-size: 14px;
      font-weight: 600;
    }
    .status.connected { background: #4CAF50; color: white; }
    .status.disconnected { background: #f44336; color: white; }
    
    .content {
      display: grid;
      grid-template-columns: 2fr 1fr;
      gap: 30px;
      margin-top: 30px;
    }
    @media (max-width: 768px) {
      .content { grid-template-columns: 1fr; }
    }
    
    .video-section {
      background: #f5f5f5;
      border-radius: 15px;
      padding: 20px;
      display: flex;
      flex-direction: column;
    }
    .video-frame {
      width: 100%;
      aspect-ratio: 4/3;
      background: #000;
      border-radius: 10px;
      display: flex;
      align-items: center;
      justify-content: center;
      overflow: hidden;
      margin-bottom: 15px;
    }
    #streamImage {
      width: 100%;
      height: 100%;
      object-fit: contain;
    }
    .no-stream {
      color: #999;
      font-size: 18px;
    }
    
    .controls {
      display: flex;
      gap: 10px;
    }
    button {
      flex: 1;
      padding: 12px 20px;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.3s;
    }
    .btn-start {
      background: #4CAF50;
      color: white;
    }
    .btn-start:hover { background: #45a049; }
    
    .btn-stop {
      background: #f44336;
      color: white;
    }
    .btn-stop:hover { background: #da190b; }
    
    .btn-save {
      background: #2196F3;
      color: white;
    }
    .btn-save:hover { background: #0b7dda; }
    
    .sidebar {
      display: flex;
      flex-direction: column;
      gap: 20px;
    }
    .panel {
      background: #f9f9f9;
      border-radius: 15px;
      padding: 20px;
      border-left: 4px solid #667eea;
    }
    .panel h3 {
      color: #333;
      margin-bottom: 10px;
      font-size: 16px;
    }
    .panel-content {
      color: #666;
      font-size: 14px;
      line-height: 1.6;
    }
    .stat {
      display: flex;
      justify-content: space-between;
      padding: 8px 0;
      border-bottom: 1px solid #eee;
    }
    .stat-label { color: #999; }
    .stat-value { font-weight: 600; color: #333; }
    
    .log-section {
      max-height: 300px;
      overflow-y: auto;
      background: #f0f0f0;
      padding: 10px;
      border-radius: 8px;
      font-family: monospace;
      font-size: 12px;
    }
    .log-entry {
      padding: 5px;
      border-bottom: 1px solid #ddd;
      color: #333;
    }
    .log-entry.info { color: #0066cc; }
    .log-entry.error { color: #cc0000; }
    .log-entry.success { color: #00aa00; }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <h1>🎥 ESP32 Face ID Dashboard</h1>
      <div class="status disconnected" id="statusBadge">Disconnected</div>
    </header>
    
    <div class="content">
      <div class="video-section">
        <div class="video-frame">
          <img id="streamImage" style="display:none;" alt="Stream">
          <div class="no-stream" id="noStream">Waiting for stream...</div>
        </div>
        <div class="controls">
          <button class="btn-start" onclick="startStream()">▶ Start Stream</button>
          <button class="btn-stop" onclick="stopStream()">⏹ Stop Stream</button>
          <button class="btn-save" onclick="saveImage()">💾 Save Frame</button>
        </div>
      </div>
      
      <div class="sidebar">
        <div class="panel">
          <h3>📊 Connection Info</h3>
          <div class="panel-content">
            <div class="stat">
              <span class="stat-label">Status:</span>
              <span class="stat-value" id="connStatus">Disconnected</span>
            </div>
            <div class="stat">
              <span class="stat-label">Frames:</span>
              <span class="stat-value" id="frameCount">0</span>
            </div>
            <div class="stat">
              <span class="stat-label">FPS:</span>
              <span class="stat-value" id="fpsValue">0</span>
            </div>
            <div class="stat">
              <span class="stat-label">Last Frame:</span>
              <span class="stat-value" id="lastFrame">N/A</span>
            </div>
          </div>
        </div>
        
        <div class="panel">
          <h3>📝 Event Log</h3>
          <div class="log-section" id="eventLog"></div>
        </div>
      </div>
    </div>
  </div>

  <script>
    const WebSocketUrl = `ws://${window.location.host}/ws`;
    let ws = null;
    let frameCount = 0;
    let frameTimestamp = Date.now();
    let currentImageData = null;

    function addLog(message, type = 'info') {
      const log = document.getElementById('eventLog');
      const entry = document.createElement('div');
      entry.className = `log-entry ${type}`;
      entry.textContent = `[${new Date().toLocaleTimeString()}] ${message}`;
      log.appendChild(entry);
      log.scrollTop = log.scrollHeight;
    }

    function updateStatus(connected) {
      const badge = document.getElementById('statusBadge');
      const status = document.getElementById('connStatus');
      if (connected) {
        badge.className = 'status connected';
        badge.textContent = 'Connected';
        status.textContent = 'Connected ✓';
        addLog('Connected to ESP32', 'success');
      } else {
        badge.className = 'status disconnected';
        badge.textContent = 'Disconnected';
        status.textContent = 'Disconnected ✗';
        addLog('Disconnected from ESP32', 'error');
      }
    }

    function connectWebSocket() {
      ws = new WebSocket(WebSocketUrl);
      ws.binaryType = 'arraybuffer';

      ws.onopen = () => {
        updateStatus(true);
        addLog('WebSocket connected', 'success');
      };

      ws.onmessage = (event) => {
        if (typeof event.data === 'string') {
          // Text message (command)
          addLog(`Server: ${event.data}`, 'info');
        } else if (event.data instanceof ArrayBuffer) {
          // Binary message (JPEG frame)
          const blob = new Blob([event.data], { type: 'image/jpeg' });
          const url = URL.createObjectURL(blob);
          
          const img = document.getElementById('streamImage');
          img.style.display = 'block';
          document.getElementById('noStream').style.display = 'none';
          img.src = url;
          
          currentImageData = event.data;
          frameCount++;
          document.getElementById('frameCount').textContent = frameCount;
          
          // Calculate FPS
          const now = Date.now();
          const fps = Math.round((frameCount * 1000) / (now - frameTimestamp + 1));
          document.getElementById('fpsValue').textContent = fps;
          
          document.getElementById('lastFrame').textContent = 
            new Date().toLocaleTimeString();
        }
      };

      ws.onerror = (error) => {
        addLog(`WebSocket error: ${error}`, 'error');
        console.error('WebSocket error:', error);
      };

      ws.onclose = () => {
        updateStatus(false);
        addLog('WebSocket disconnected', 'error');
        setTimeout(connectWebSocket, 3000);
      };
    }

    function startStream() {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send('start_stream');
        addLog('Sent: start_stream', 'info');
      }
    }

    function stopStream() {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send('stop_stream');
        addLog('Sent: stop_stream', 'info');
      }
    }

    function saveImage() {
      if (!currentImageData) {
        alert('No frame to save');
        return;
      }
      
      const blob = new Blob([currentImageData], { type: 'image/jpeg' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `frame_${Date.now()}.jpg`;
      a.click();
      URL.revokeObjectURL(url);
      addLog('Frame saved', 'success');
    }

    // Initialize
    updateStatus(false);
    connectWebSocket();
  </script>
</body>
</html>
"""


@app.route("/")
def dashboard():
    """Serve dashboard"""
    return render_template_string(DASHBOARD_HTML)


@sock.route("/ws")
def websocket_handler(ws):
    """Handle WebSocket connection from ESP32"""
    client_id = request.remote_addr
    clients[client_id] = {
        'ws': ws,
        'connected_at': datetime.now(),
        'frames_received': 0
    }
    
    logger.info(f"ESP32 client connected: {client_id}")
    
    try:
        # Send initial command
        ws.send("start_stream")
        
        while True:
            data = ws.receive(raw=True)
            
            if data is None:
                break
                
            if isinstance(data, bytes):
                # Binary data - JPEG frame
                clients[client_id]['frames_received'] += 1
                
                # Optionally save frames to disk
                # frame_path = UPLOAD_DIR / f"frame_{datetime.now().isoformat()}.jpg"
                # with open(frame_path, 'wb') as f:
                #     f.write(data)
                
                logger.debug(f"Received JPEG frame: {len(data)} bytes from {client_id}")
            else:
                # Text data - command or status
                logger.info(f"Received text: {data} from {client_id}")
                
    except Exception as e:
        logger.error(f"WebSocket error from {client_id}: {e}")
    finally:
        if client_id in clients:
            del clients[client_id]
        logger.info(f"ESP32 client disconnected: {client_id}")


@app.route("/api/clients")
def get_clients():
    """Get connected clients info"""
    clients_info = []
    for client_id, info in clients.items():
        clients_info.append({
            'ip': client_id,
            'connected_at': info['connected_at'].isoformat(),
            'frames_received': info['frames_received']
        })
    return {
        'total_clients': len(clients),
        'clients': clients_info
    }


if __name__ == "__main__":
    logger.info("Starting ESP32 Face ID Server...")
    logger.info(f"Dashboard: http://localhost:8080/")
    logger.info(f"WebSocket: ws://localhost:8080/ws")
    
    app.run(host='0.0.0.0', port=8080, debug=True)
