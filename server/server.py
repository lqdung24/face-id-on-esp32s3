"""
ESP32 Face-ID Receiver Server (Raw WebSocket Version)
------------------------------------------------------
"""

import os
import json
import base64
from datetime import datetime
from flask import Flask, render_template_string
from flask_sock import Sock

app = Flask(__name__)
sock = Sock(app)

clients = set()

DASHBOARD_HTML = """
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>HUST Smart Lock – WebSocket Dashboard</title>
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;600;700&display=swap" rel="stylesheet">
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
    :root { --bg: #0d0f14; --surface: #161a24; --card: #1e2330; --border: #2a3045; --accent: #4f8ef7; --success: #22c55e; --danger: #ef4444; --text: #e2e8f0; --muted: #64748b; }
    body { font-family: 'Inter', sans-serif; background: var(--bg); color: var(--text); padding: 20px; }
    header { display: flex; align-items: center; gap: 14px; margin-bottom: 25px; }
    header .logo { width: 42px; height: 42px; background: linear-gradient(135deg, var(--accent), #a855f7); border-radius: 12px; display: flex; align-items: center; justify-content: center; font-size: 22px; }
    header h1 { font-size: 1.4rem; font-weight: 700; }
    header p  { font-size: 0.8rem; color: var(--muted); margin-top: 2px; }
    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; max-width: 1200px; margin: 0 auto; }
    @media (max-width: 768px) { .grid { grid-template-columns: 1fr; } }
    .card { background: var(--card); border: 1px solid var(--border); border-radius: 16px; padding: 20px; }
    .card h2 { font-size: 0.85rem; font-weight: 600; color: var(--muted); text-transform: uppercase; letter-spacing: .08em; margin-bottom: 14px; position: relative; }
    .live-feed { aspect-ratio: 4/3; background: #000; border-radius: 12px; overflow: hidden; display: flex; align-items: center; justify-content: center; position: relative; }
    #live-img { width: 100%; height: 100%; object-fit: contain; transform: scaleX(-1); display: none; }
    .btn { background: var(--accent); color: white; border: none; padding: 10px 16px; border-radius: 8px; cursor: pointer; font-weight: 600; font-size: 0.85rem; transition: background 0.2s; }
    .btn:hover { background: #3b76d9; }
    .btn.active { background: var(--danger); }
    .hb-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    .hb-item { background: var(--surface); border-radius: 10px; padding: 12px 14px; }
    .hb-item .label { font-size: 0.72rem; color: var(--muted); }
    .hb-item .value { font-size: 1.2rem; font-weight: 700; margin-top: 4px; }
    .dot { display: inline-block; width: 8px; height: 8px; border-radius: 50%; margin-right: 6px; }
    .dot.online { background: var(--success); box-shadow: 0 0 10px var(--success); }
    .dot.offline { background: var(--danger); }
    #event-list { list-style: none; max-height: 300px; overflow-y: auto; scrollbar-width: thin; }
    #event-list li { display: flex; gap: 10px; padding: 10px 0; border-bottom: 1px solid var(--border); font-size: 0.85rem; }
    .ev-icon { width: 30px; height: 30px; border-radius: 8px; display: flex; align-items: center; justify-content: center; font-size: 16px; background: #333; }
    .ev-meta strong { display: block; }
    .ev-meta span { color: var(--muted); font-size: 0.75rem; }
  </style>
</head>
<body>
<header>
  <div class="logo">🔒</div>
  <div>
    <h1>HUST Smart Lock Dashboard</h1>
    <p>WebSocket Live Connection</p>
  </div>
</header>
<div class="grid">
  <!-- Live Camera -->
  <div class="card">
    <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:14px;">
      <h2 style="margin:0;">📷 Live Camera</h2>
      <div>
        <button id="ai-btn" class="btn" style="background:#a855f7; margin-right: 10px;" onclick="toggleAI()">Run AI</button>
        <button id="stream-btn" class="btn" onclick="toggleStream()">Start Stream</button>
      </div>
    </div>
    <div class="live-feed">
      <div id="no-feed" style="color:var(--muted); font-size:0.85rem;">Chưa có ảnh…</div>
      <img id="live-img" alt="live frame">
    </div>
  </div>
  <!-- Status -->
  <div class="card">
    <h2><span class="dot offline" id="status-dot"></span> Device Status</h2>
    <div class="hb-grid">
      <div class="hb-item">
        <div class="label">Faces Enrolled</div>
        <div class="value" id="val-faces">–</div>
      </div>
      <div class="hb-item">
        <div class="label">Last Seen</div>
        <div class="value" id="val-seen" style="font-size:0.9rem">–</div>
      </div>
      <div class="hb-item" style="grid-column: 1 / -1;">
        <div class="label">Last Recognition</div>
        <div class="value" id="val-recog" style="color:var(--success)">–</div>
      </div>
    </div>
  </div>
  <!-- Events -->
  <div class="card" style="grid-column: 1 / -1;">
    <h2>📋 Event Log</h2>
    <ul id="event-list"></ul>
  </div>
</div>
<script>
  let ws;
  let streaming = false;

  function connect() {
    ws = new WebSocket("ws://" + location.host + "/ws");
    ws.onopen = () => { addEvent('server', '🟢 Connected to Server', 'WebSocket up'); };
    ws.onclose = () => { setTimeout(connect, 2000); };
    ws.onmessage = (e) => {
      try {
        const d = JSON.parse(e.data);
        if (d.type === 'esp_status') {
          document.getElementById('status-dot').className = 'dot online';
          document.getElementById('val-faces').textContent = d.faces_enrolled ?? '–';
          document.getElementById('val-seen').textContent = new Date().toLocaleTimeString();
          addEvent('status', '💚 Heartbeat', 'Faces: ' + d.faces_enrolled);
        } else if (d.type === 'esp_event') {
          document.getElementById('val-recog').textContent = d.name;
          addEvent('face', '✅ Recognised: ' + d.name, 'Name Matched');
        } else if (d.type === 'esp_frame') {
          const img = document.getElementById('live-img');
          img.src = 'data:image/jpeg;base64,' + d.frame_b64;
          img.style.display = 'block';
          document.getElementById('no-feed').style.display = 'none';
        }
      } catch(err) { console.error(err); }
    };
  }

  function toggleStream() {
    streaming = !streaming;
    const btn = document.getElementById('stream-btn');
    btn.textContent = streaming ? 'Stop Stream' : 'Start Stream';
    btn.className = streaming ? 'btn active' : 'btn';
    if(ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type: "cmd_to_esp", action: streaming ? 'start_stream' : 'stop_stream' }));
    }
  }

  let ai_running = false;
  function toggleAI() {
    ai_running = !ai_running;
    const btn = document.getElementById('ai-btn');
    btn.textContent = ai_running ? 'Stop AI' : 'Run AI';
    if(ai_running) {
        btn.style.background = 'var(--danger)';
    } else {
        btn.style.background = '#a855f7';
    }
    if(ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type: "cmd_to_esp", action: ai_running ? 'start_ai' : 'stop_ai' }));
    }
  }

  function addEvent(type, title, detail) {
    const ul = document.getElementById('event-list');
    const li = document.createElement('li');
    let icon = 'ℹ️'; if(type === 'face') icon = '🧑'; if(type === 'status') icon = '💚';
    li.innerHTML = `<div class="ev-icon">${icon}</div><div class="ev-meta"><strong>${title}</strong><span>${detail}</span></div>`;
    ul.insertBefore(li, ul.firstChild);
    if(ul.children.length > 50) ul.lastChild.remove();
  }
  
  connect();
</script>
</body>
</html>
"""

@app.route("/")
def dashboard():
    return render_template_string(DASHBOARD_HTML)

@sock.route("/ws")
def handle_ws(ws):
    clients.add(ws)
    print(f"[WS] Client connected. Total: {len(clients)}")
    try:
        while True:
            msg = ws.receive()
            if msg is None:
                break

            # --- Binary frame: ESP32 gửi raw JPEG bytes ---
            if isinstance(msg, bytes):
                # Convert sang base64 JSON để browser hiển thị
                b64 = base64.b64encode(msg).decode('ascii')
                payload = json.dumps({"type": "esp_frame", "frame_b64": b64})
                for client in list(clients):
                    if client != ws:
                        try:
                            client.send(payload)
                        except Exception:
                            clients.discard(client)
                continue

            # --- Text frame: JSON từ browser hoặc ESP32 ---
            try:
                # Forward nguyên vẹn cho các client khác
                for client in list(clients):
                    if client != ws:
                        try:
                            client.send(msg)
                        except Exception:
                            clients.discard(client)

                data = json.loads(msg)
                if data.get('type') != 'esp_frame':
                    print(f"[WS] {data.get('type')} -> {len(clients)-1} clients")
            except Exception as e:
                print(f"[WS] Error: {e}")
    finally:
        clients.discard(ws)
        print(f"[WS] Client disconnected. Total: {len(clients)}")

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
