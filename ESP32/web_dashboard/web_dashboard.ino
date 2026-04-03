/*  web_dashboard.ino — ESP32 Web Dashboard Bridge
 *
 *  Receives JSON diagnostic packets from STM32F429 over UART2 (115200 8N1)
 *  and serves them to a browser as a real-time dashboard.
 *
 *  Wiring:
 *  ┌──────────────────────┬──────────────────────────────────────┐
 *  │ STM32F429 Pin        │ ESP32 Pin                            │
 *  ├──────────────────────┼──────────────────────────────────────┤
 *  │ PA2  (USART2 TX)     │ GPIO16 (RX2) — 3.3 V logic, no shift│
 *  │ PA3  (USART2 RX)     │ GPIO17 (TX2)                        │
 *  │ GND                  │ GND  (common ground is essential!)   │
 *  └──────────────────────┴──────────────────────────────────────┘
 *
 *  WiFi: The ESP32 starts as an Access Point.
 *    SSID: ECU_Dashboard
 *    Pass: ecu12345
 *    URL:  http://192.168.4.1
 *
 *    To connect to an existing network instead, set WIFI_MODE to STATION
 *    and fill in WIFI_SSID / WIFI_PASS below.
 *
 *  Required libraries (Arduino IDE Library Manager):
 *    - ArduinoJson  (by Benoit Blanchon, v6.x)
 *
 *  Dashboard features:
 *    - Live vehicle data: RPM, Speed, Temp, Battery, Throttle
 *    - CAN bus health: TEC/REC, bus-off, error-passive, frame rate, timeout
 *    - RTOS health: free heap, task count, stack watermarks
 *    - Updates every 500 ms via SSE (Server-Sent Events)
 *    - Serial monitor echo of all received JSON lines
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

/* ---- WiFi configuration ---------------------------------------------- */
#define WIFI_MODE_AP      1    /* 1 = Access Point, 0 = Station */
#define AP_SSID           "ECU_Dashboard"
#define AP_PASS           "ecu12345"
#define STA_SSID          "YourSSID"     /* Used if WIFI_MODE_AP = 0 */
#define STA_PASS          "YourPassword"

/* ---- UART from STM32 ------------------------------------------------- */
#define STM32_UART        Serial2
#define STM32_BAUD        115200
#define STM32_RX_PIN      16
#define STM32_TX_PIN      17
#define UART_BUF_SIZE     512

/* ---- SSE --------------------------------------------------------------- */
#define SSE_MAX_CLIENTS   4

/* ====================================================================== */

WebServer server(80);

/* Shared data (updated by UART task, read by HTTP handlers) */
struct DiagData {
    uint32_t uptime_ms;
    uint16_t rpm;
    uint8_t  speed_kmh;
    int16_t  temp_c;
    uint16_t battery_mv;
    uint8_t  throttle_pct;
    /* CAN */
    bool     can_ok;
    uint8_t  tec;
    uint8_t  rec;
    uint32_t fps;
    uint32_t rx_frames;
    uint32_t rx_errors;
    bool     timeout;
    bool     bus_off;
    bool     error_passive;
    uint8_t  lec;
    /* RTOS */
    uint32_t free_heap;
    uint32_t min_heap;
    uint8_t  num_tasks;
    char     wm_json[128];     /* Pre-formatted watermark JSON array */
    bool     valid;
} diag;

/* SSE client tracking */
struct SSEClient {
    WiFiClient client;
    bool       active;
} sse_clients[SSE_MAX_CLIENTS];

portMUX_TYPE diag_mux = portMUX_INITIALIZER_UNLOCKED;

/* ======================================================================
 * Dashboard HTML (served from flash)
 * ==================================================================== */
static const char DASHBOARD_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ECU Diagnostic Dashboard</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{font-family:'Segoe UI',Arial,sans-serif;background:#0d1117;color:#e6edf3;min-height:100vh}
  header{background:#161b22;border-bottom:1px solid #30363d;padding:16px 24px;display:flex;align-items:center;justify-content:space-between}
  header h1{font-size:1.3rem;color:#58a6ff}
  #conn{font-size:.8rem;padding:4px 10px;border-radius:12px;background:#21262d}
  #conn.live{background:#1a4731;color:#3fb950}
  #conn.dead{background:#3d1f1f;color:#f85149}
  .grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(160px,1fr));gap:14px;padding:20px}
  .section{padding:0 20px 8px;font-size:.75rem;color:#8b949e;text-transform:uppercase;letter-spacing:.08em}
  .card{background:#161b22;border:1px solid #30363d;border-radius:10px;padding:16px 14px;transition:border-color .3s}
  .card:hover{border-color:#58a6ff}
  .card .label{font-size:.7rem;color:#8b949e;margin-bottom:6px;text-transform:uppercase;letter-spacing:.05em}
  .card .value{font-size:1.7rem;font-weight:700;line-height:1}
  .card .unit{font-size:.75rem;color:#8b949e;margin-top:3px}
  .card.ok   .value{color:#3fb950}
  .card.warn .value{color:#d29922}
  .card.err  .value{color:#f85149}
  .card.neu  .value{color:#58a6ff}
  .bar-bg{height:6px;background:#21262d;border-radius:3px;margin-top:8px;overflow:hidden}
  .bar-fill{height:100%;border-radius:3px;transition:width .5s,background .5s}
  .wm-table{width:100%;font-size:.75rem;border-collapse:collapse}
  .wm-table td{padding:3px 6px;border-bottom:1px solid #21262d}
  .wm-table td:last-child{text-align:right;color:#3fb950}
  footer{text-align:center;padding:14px;font-size:.7rem;color:#484f58}
</style>
</head>
<body>
<header>
  <h1>&#9881; ECU Diagnostic Dashboard</h1>
  <span id="conn" class="dead">&#11044; Connecting...</span>
</header>

<!-- Vehicle Data -->
<p class="section" style="margin-top:14px">Vehicle Data</p>
<div class="grid">
  <div class="card neu" id="card-rpm">
    <div class="label">Engine RPM</div>
    <div class="value" id="val-rpm">—</div>
    <div class="unit">RPM</div>
    <div class="bar-bg"><div class="bar-fill" id="bar-rpm" style="width:0%;background:#58a6ff"></div></div>
  </div>
  <div class="card neu" id="card-spd">
    <div class="label">Speed</div>
    <div class="value" id="val-spd">—</div>
    <div class="unit">km/h</div>
    <div class="bar-bg"><div class="bar-fill" id="bar-spd" style="width:0%;background:#58a6ff"></div></div>
  </div>
  <div class="card neu" id="card-tmp">
    <div class="label">Engine Temp</div>
    <div class="value" id="val-tmp">—</div>
    <div class="unit">°C</div>
  </div>
  <div class="card neu" id="card-bat">
    <div class="label">Battery</div>
    <div class="value" id="val-bat">—</div>
    <div class="unit">V</div>
  </div>
  <div class="card neu" id="card-tps">
    <div class="label">Throttle</div>
    <div class="value" id="val-tps">—</div>
    <div class="unit">%</div>
    <div class="bar-bg"><div class="bar-fill" id="bar-tps" style="width:0%;background:#58a6ff"></div></div>
  </div>
</div>

<!-- CAN Health -->
<p class="section">CAN Bus Health</p>
<div class="grid">
  <div class="card" id="card-can">
    <div class="label">Bus Status</div>
    <div class="value" id="val-can">—</div>
  </div>
  <div class="card neu" id="card-fps">
    <div class="label">Frame Rate</div>
    <div class="value" id="val-fps">—</div>
    <div class="unit">frames/s</div>
  </div>
  <div class="card neu" id="card-tec">
    <div class="label">TEC</div>
    <div class="value" id="val-tec">—</div>
    <div class="unit">Tx Errors</div>
  </div>
  <div class="card neu" id="card-rec">
    <div class="label">REC</div>
    <div class="value" id="val-rec">—</div>
    <div class="unit">Rx Errors</div>
  </div>
  <div class="card neu" id="card-rx">
    <div class="label">Total Frames</div>
    <div class="value" id="val-rx">—</div>
  </div>
  <div class="card neu" id="card-lec">
    <div class="label">Last Error</div>
    <div class="value" id="val-lec">—</div>
    <div class="unit">LEC code</div>
  </div>
</div>

<!-- RTOS Health -->
<p class="section">RTOS Health</p>
<div class="grid">
  <div class="card neu">
    <div class="label">Free Heap</div>
    <div class="value" id="val-heap">—</div>
    <div class="unit">bytes</div>
  </div>
  <div class="card neu">
    <div class="label">Min Heap</div>
    <div class="value" id="val-mheap">—</div>
    <div class="unit">bytes</div>
  </div>
  <div class="card neu">
    <div class="label">Tasks</div>
    <div class="value" id="val-tasks">—</div>
  </div>
  <div class="card neu">
    <div class="label">Uptime</div>
    <div class="value" id="val-uptime">—</div>
    <div class="unit">seconds</div>
  </div>
</div>
<div style="padding:0 20px 14px">
  <div class="card neu" style="max-width:400px">
    <div class="label" style="margin-bottom:8px">Stack Watermarks (words free)</div>
    <table class="wm-table" id="wm-table"><tr><td colspan="2">—</td></tr></table>
  </div>
</div>

<footer>STM32F429 → USART2 → ESP32 | Update rate: 500 ms</footer>

<script>
const LEC_NAMES = ["No error","Bit dominant","Bit recessive","Bit error","Stuff error","CRC error","Form error","ACK error"];
let evtSrc;

function connect() {
  evtSrc = new EventSource('/events');
  evtSrc.onopen = () => {
    document.getElementById('conn').textContent = '● Live';
    document.getElementById('conn').className = 'live';
  };
  evtSrc.onerror = () => {
    document.getElementById('conn').textContent = '● Disconnected';
    document.getElementById('conn').className = 'dead';
    evtSrc.close();
    setTimeout(connect, 3000);
  };
  evtSrc.onmessage = e => {
    try { update(JSON.parse(e.data)); } catch(ex) {}
  };
}

function setCard(id, cls) {
  const c = document.getElementById('card-' + id);
  c.className = 'card ' + cls;
}
function setBar(id, pct, color) {
  const b = document.getElementById('bar-' + id);
  if (b) { b.style.width = pct + '%'; b.style.background = color; }
}

function update(d) {
  // Vehicle data
  document.getElementById('val-rpm').textContent  = d.rpm;
  document.getElementById('val-spd').textContent  = d.spd;
  document.getElementById('val-tmp').textContent  = d.tmp;
  document.getElementById('val-bat').textContent  = (d.bat / 1000).toFixed(2);
  document.getElementById('val-tps').textContent  = d.tps;
  setCard('rpm', d.rpm > 6500 ? 'warn' : 'neu');
  setCard('tmp', d.tmp > 105 ? 'err' : d.tmp > 90 ? 'warn' : 'ok');
  setCard('bat', d.bat < 11000 ? 'err' : d.bat < 12000 ? 'warn' : 'ok');
  setBar('rpm', Math.min(d.rpm / 80, 100), d.rpm > 6500 ? '#d29922' : '#58a6ff');
  setBar('spd', Math.min(d.spd / 2,  100), '#58a6ff');
  setBar('tps', d.tps,                     '#58a6ff');

  // CAN health
  const can = d.can;
  const canStatus = can.boff ? 'BUS-OFF' : can.to ? 'TIMEOUT' : can.ok ? 'ACTIVE' : 'IDLE';
  document.getElementById('val-can').textContent = canStatus;
  setCard('can', can.boff ? 'err' : can.to ? 'warn' : can.ok ? 'ok' : 'neu');
  document.getElementById('val-fps').textContent  = can.fps;
  document.getElementById('val-tec').textContent  = can.tec;
  document.getElementById('val-rec').textContent  = can.rec;
  document.getElementById('val-rx').textContent   = can.rx;
  document.getElementById('val-lec').textContent  = LEC_NAMES[can.lec] || can.lec;
  setCard('tec', can.tec >= 96 ? 'err' : can.tec > 0 ? 'warn' : 'ok');
  setCard('rec', can.rec >= 96 ? 'err' : can.rec > 0 ? 'warn' : 'ok');

  // RTOS
  const r = d.rtos;
  document.getElementById('val-heap').textContent   = r.heap.toLocaleString();
  document.getElementById('val-mheap').textContent  = r.mheap.toLocaleString();
  document.getElementById('val-tasks').textContent  = r.ntasks;
  document.getElementById('val-uptime').textContent = (d.t / 1000).toFixed(1);

  // Watermarks table
  let html = '';
  if (r.wm && r.wm.length) {
    r.wm.forEach(([name, words]) => {
      const cls = words < 20 ? 'style="color:#f85149"' : words < 50 ? 'style="color:#d29922"' : '';
      html += `<tr><td>${name}</td><td ${cls}>${words}</td></tr>`;
    });
  }
  document.getElementById('wm-table').innerHTML = html || '<tr><td colspan="2">—</td></tr>';
}

connect();
</script>
</body>
</html>
)rawhtml";

/* ======================================================================
 * SSE helpers
 * ==================================================================== */
static void sse_send_all(const String &data)
{
    for (int i = 0; i < SSE_MAX_CLIENTS; i++)
    {
        if (!sse_clients[i].active) { continue; }
        if (!sse_clients[i].client.connected())
        {
            sse_clients[i].active = false;
            continue;
        }
        sse_clients[i].client.print("data: ");
        sse_clients[i].client.print(data);
        sse_clients[i].client.print("\n\n");
    }
}

/* ======================================================================
 * HTTP handlers
 * ==================================================================== */
static void handle_root(void)
{
    server.send_P(200, "text/html", DASHBOARD_HTML);
}

static void handle_events(void)
{
    /* Upgrade the connection to an SSE stream */
    WiFiClient client = server.client();
    client.print("HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/event-stream\r\n"
                 "Cache-Control: no-cache\r\n"
                 "Connection: keep-alive\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "\r\n");

    for (int i = 0; i < SSE_MAX_CLIENTS; i++)
    {
        if (!sse_clients[i].active)
        {
            sse_clients[i].client = client;
            sse_clients[i].active = true;
            break;
        }
    }
}

static void handle_data_json(void)
{
    /* One-shot JSON endpoint (useful for debugging / non-SSE clients) */
    portENTER_CRITICAL(&diag_mux);
    DiagData snap = diag;
    portEXIT_CRITICAL(&diag_mux);

    char buf[400];
    snprintf(buf, sizeof(buf),
             "{\"t\":%lu,\"rpm\":%u,\"spd\":%u,\"tmp\":%d,\"bat\":%u,\"tps\":%u,"
             "\"can\":{\"ok\":%u,\"tec\":%u,\"rec\":%u,\"fps\":%lu,\"rx\":%lu,"
             "\"err\":%lu,\"to\":%u,\"boff\":%u,\"erp\":%u,\"lec\":%u},"
             "\"rtos\":{\"heap\":%lu,\"mheap\":%lu,\"ntasks\":%u,\"wm\":%s}}",
             (unsigned long)snap.uptime_ms,
             snap.rpm, snap.speed_kmh, (int)snap.temp_c,
             snap.battery_mv, snap.throttle_pct,
             (unsigned)snap.can_ok, snap.tec, snap.rec,
             (unsigned long)snap.fps, (unsigned long)snap.rx_frames,
             (unsigned long)snap.rx_errors,
             (unsigned)snap.timeout, (unsigned)snap.bus_off,
             (unsigned)snap.error_passive, snap.lec,
             (unsigned long)snap.free_heap, (unsigned long)snap.min_heap,
             snap.num_tasks, snap.wm_json);

    server.send(200, "application/json", buf);
}

/* ======================================================================
 * parseAndStore — parses one JSON line from STM32 into diag struct.
 * Returns true on success.
 * ==================================================================== */
static bool parseAndStore(const char *line)
{
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, line);
    if (err) { return false; }

    DiagData tmp;
    tmp.uptime_ms    = doc["t"]   | 0UL;
    tmp.rpm          = doc["rpm"] | 0;
    tmp.speed_kmh    = doc["spd"] | 0;
    tmp.temp_c       = doc["tmp"] | 0;
    tmp.battery_mv   = doc["bat"] | 0;
    tmp.throttle_pct = doc["tps"] | 0;

    JsonObject can    = doc["can"];
    tmp.can_ok        = (can["ok"]   | 0) != 0;
    tmp.tec           = can["tec"]   | 0;
    tmp.rec           = can["rec"]   | 0;
    tmp.fps           = can["fps"]   | 0UL;
    tmp.rx_frames     = can["rx"]    | 0UL;
    tmp.rx_errors     = can["err"]   | 0UL;
    tmp.timeout       = (can["to"]   | 0) != 0;
    tmp.bus_off       = (can["boff"] | 0) != 0;
    tmp.error_passive = (can["erp"]  | 0) != 0;
    tmp.lec           = can["lec"]   | 0;

    JsonObject rtos   = doc["rtos"];
    tmp.free_heap     = rtos["heap"]   | 0UL;
    tmp.min_heap      = rtos["mheap"]  | 0UL;
    tmp.num_tasks     = rtos["ntasks"] | 0;

    /* Re-serialise watermark array as a compact JSON string */
    JsonArray wm = rtos["wm"];
    if (!wm.isNull())
    {
        serializeJson(wm, tmp.wm_json, sizeof(tmp.wm_json));
    }
    else
    {
        strcpy(tmp.wm_json, "[]");
    }

    tmp.valid = true;

    portENTER_CRITICAL(&diag_mux);
    diag = tmp;
    portEXIT_CRITICAL(&diag_mux);
    return true;
}

/* ======================================================================
 * buildSSEPayload — creates the JSON string pushed over SSE.
 * Reuses the same format so the dashboard JS works with both SSE and /data.
 * ==================================================================== */
static String buildSSEPayload(void)
{
    portENTER_CRITICAL(&diag_mux);
    DiagData snap = diag;
    portEXIT_CRITICAL(&diag_mux);

    char buf[400];
    snprintf(buf, sizeof(buf),
             "{\"t\":%lu,\"rpm\":%u,\"spd\":%u,\"tmp\":%d,\"bat\":%u,\"tps\":%u,"
             "\"can\":{\"ok\":%u,\"tec\":%u,\"rec\":%u,\"fps\":%lu,\"rx\":%lu,"
             "\"err\":%lu,\"to\":%u,\"boff\":%u,\"erp\":%u,\"lec\":%u},"
             "\"rtos\":{\"heap\":%lu,\"mheap\":%lu,\"ntasks\":%u,\"wm\":%s}}",
             (unsigned long)snap.uptime_ms,
             snap.rpm, snap.speed_kmh, (int)snap.temp_c,
             snap.battery_mv, snap.throttle_pct,
             (unsigned)snap.can_ok, snap.tec, snap.rec,
             (unsigned long)snap.fps, (unsigned long)snap.rx_frames,
             (unsigned long)snap.rx_errors,
             (unsigned)snap.timeout, (unsigned)snap.bus_off,
             (unsigned)snap.error_passive, snap.lec,
             (unsigned long)snap.free_heap, (unsigned long)snap.min_heap,
             snap.num_tasks, snap.wm_json);
    return String(buf);
}

/* ======================================================================
 * setup
 * ==================================================================== */
void setup()
{
    Serial.begin(115200);
    Serial.println("\n=== ECU Diagnostic Dashboard ===");

    /* UART from STM32 */
    STM32_UART.begin(STM32_BAUD, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);

    /* Zero diagnostics */
    memset(&diag, 0, sizeof(diag));
    strcpy(diag.wm_json, "[]");

    /* WiFi */
#if WIFI_MODE_AP
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.print("AP started: ");
    Serial.println(AP_SSID);
    Serial.print("Dashboard URL: http://");
    Serial.println(WiFi.softAPIP());
#else
    WiFi.begin(STA_SSID, STA_PASS);
    Serial.print("Connecting to ");
    Serial.print(STA_SSID);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print('.');
    }
    Serial.println();
    Serial.print("Dashboard URL: http://");
    Serial.println(WiFi.localIP());
#endif

    /* HTTP routes */
    server.on("/",      HTTP_GET, handle_root);
    server.on("/events",HTTP_GET, handle_events);
    server.on("/data",  HTTP_GET, handle_data_json);
    server.begin();
    Serial.println("HTTP server started.");
}

/* ======================================================================
 * loop
 * ==================================================================== */
static char   uart_buf[UART_BUF_SIZE];
static int    uart_pos     = 0;
static uint32_t last_sse_ms = 0;

void loop()
{
    /* ---- Handle HTTP clients ---------------------------------------- */
    server.handleClient();

    /* ---- Read UART from STM32 --------------------------------------- */
    while (STM32_UART.available())
    {
        char c = (char)STM32_UART.read();

        if (c == '\n')
        {
            if (uart_pos > 0)
            {
                uart_buf[uart_pos] = '\0';
                Serial.print("[STM32] ");
                Serial.println(uart_buf);          /* Echo to serial monitor */
                parseAndStore(uart_buf);
            }
            uart_pos = 0;
        }
        else if (c != '\r')
        {
            if (uart_pos < (UART_BUF_SIZE - 1))
            {
                uart_buf[uart_pos++] = c;
            }
            else
            {
                /* Buffer overflow — reset */
                uart_pos = 0;
            }
        }
    }

    /* ---- Push SSE event every 500 ms -------------------------------- */
    uint32_t now = millis();
    if ((now - last_sse_ms) >= 500UL)
    {
        last_sse_ms = now;
        String payload = buildSSEPayload();
        sse_send_all(payload);
    }
}
