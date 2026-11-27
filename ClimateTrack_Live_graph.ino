// ClimateTrack: ESP32 + DHT11 + 16x2 I2C LCD + Wi-Fi + CSV + live graphs
// Arduino ESP32 board package; DHT by Adafruit; LiquidCrystal_I2C by Marco Schwartz
// Keep passwords 


#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

const char* WIFI_NAME = "YOUR_HOTSPOT_NAME";
const char* WIFI_PASSWORD = "YOUR_HOTSPOT_PASSWORD";

#define DHTPIN 4
#define DHTTYPE DHT11

LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);

const unsigned long READ_INTERVAL_MS = 2500;
const unsigned long LOG_INTERVAL_MS = 60000;
const size_t MAX_LOG_BYTES = 150000;
const char* LOG_PATH = "/readings.csv";

float temperatureF = NAN;
float humidity = NAN;
unsigned long lastReadingMs = 0;
unsigned long lastLogMs = 0;
bool serverStarted = false;
bool fileSystemReady = false;
bool firstLogWritten = false;
bool logFull = false;

const char DASHBOARD[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ClimateTrack | Live Dashboard</title>
<style>
  * { box-sizing: border-box; }
  body { margin: 0; padding: 20px 12px; background: #edf3f8; color: #1b344a; font-family: Arial, sans-serif; }
  main { max-width: 760px; margin: auto; }
  header { text-align: center; margin-bottom: 16px; }
  h1 { margin: 3px 0; }
  header p { margin-top: 5px; color: #657b8e; }
  .cards { display: grid; grid-template-columns: repeat(2,minmax(0,1fr)); gap: 12px; }
  .card { background: white; border-radius: 15px; padding: 19px; margin-bottom: 12px; box-shadow: 0 2px 12px #13334d10; }
  .metric { text-align: center; }
  .label { font-size: 15px; color: #657b8e; }
  .value { margin: 10px 0 0; font-size: clamp(23px,6vw,36px); font-weight: bold; }
  h2 { font-size: 18px; margin: 0 0 10px; }
  canvas { display: block; width: 100%; height: 215px; }
  .meta { font-size: 13px; color: #657b8e; text-align: center; }
  .center { text-align: center; }
  a { display: inline-block; background: #176bb4; color: white; padding: 12px 18px; border-radius: 9px; font-weight: bold; text-decoration: none; }
  #status { min-height: 20px; }
  @media (max-width: 400px) { .card { padding: 13px; } }
</style>
</head>
<body>
<main>
  <header>
    <h1>ClimateTrack</h1>
    <p>ESP32 Temperature &amp; Humidity Monitor</p>
  </header>
  <div class="cards">
    <div class="card metric"><div class="label">Temperature</div><div id="temp" class="value">--</div></div>
    <div class="card metric"><div class="label">Humidity</div><div id="humid" class="value">--</div></div>
  </div>
  <div class="card">
    <h2>Temperature history (&deg;F)</h2>
    <canvas id="tempGraph" role="img" aria-label="Live temperature history graph"></canvas>
  </div>
  <div class="card">
    <h2>Humidity history (%)</h2>
    <canvas id="humidityGraph" role="img" aria-label="Live humidity history graph"></canvas>
  </div>
  <div class="card center">
    <p id="status" class="meta">Connecting...</p>
    <a href="/download">Download CSV log</a>
    <p id="logState" class="meta">Checking CSV logging...</p>
    <p class="meta">Graphs update every 3 seconds and show up to 120 browser samples. Refreshing clears the graphs, not the stored CSV log.</p>
  </div>
</main>
<script>
const MAX_POINTS = 120;
const points = [];

function drawGraph(canvasId, field, color, unit) {
  const canvas = document.getElementById(canvasId);
  const ctx = canvas.getContext('2d');
  const width = Math.max(200, Math.round(canvas.getBoundingClientRect().width));
  const height = 215;
  const scale = window.devicePixelRatio || 1;
  canvas.width = Math.round(width * scale);
  canvas.height = Math.round(height * scale);
  ctx.setTransform(scale, 0, 0, scale, 0, 0);
  ctx.clearRect(0, 0, width, height);

  const left = 48, right = width - 12, top = 12, bottom = height - 35;
  const values = points.map(p => p[field]);
  let minVal = values.length ? Math.min(...values) : 0;
  let maxVal = values.length ? Math.max(...values) : 1;
  const range = Math.max(maxVal - minVal, 1);
  minVal -= range * 0.12;
  maxVal += range * 0.12;

  ctx.font = '12px Arial';
  ctx.textAlign = 'right';
  ctx.strokeStyle = '#e4edf4';
  ctx.fillStyle = '#64788a';
  for (let i = 0; i < 5; i++) {
    const y = top + (bottom - top) * i / 4;
    ctx.beginPath(); ctx.moveTo(left, y); ctx.lineTo(right, y); ctx.stroke();
    ctx.fillText((maxVal - (maxVal - minVal) * i / 4).toFixed(1), left - 6, y + 4);
  }

  if (!points.length) {
    ctx.textAlign = 'center';
    ctx.fillText('Waiting for sensor readings...', (left + right) / 2, height / 2);
    return;
  }

  const xPos = i => left + (right - left) * (points.length === 1 ? 0.5 : i / (points.length - 1));
  const yPos = v => bottom - (v - minVal) / (maxVal - minVal) * (bottom - top);

  ctx.beginPath();
  ctx.strokeStyle = color;
  ctx.lineWidth = 2.6;
  points.forEach((p, i) => {
    const x = xPos(i), y = yPos(p[field]);
    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  });
  ctx.stroke();

  const last = points.length - 1;
  ctx.beginPath();
  ctx.fillStyle = color;
  ctx.arc(xPos(last), yPos(points[last][field]), 3.5, 0, Math.PI * 2);
  ctx.fill();

  ctx.fillStyle = '#64788a';
  ctx.textAlign = 'left';
  ctx.fillText('+' + points[0].uptime + 's', left, height - 13);
  ctx.textAlign = 'right';
  ctx.fillText('+' + points[last].uptime + 's', right, height - 13);
}

function redraw() {
  drawGraph('tempGraph', 'temperatureF', '#cf5828', 'F');
  drawGraph('humidityGraph', 'humidity', '#1875be', '%');
}

async function update() {
  try {
    const response = await fetch('/readings', {cache: 'no-store'});
    if (!response.ok) throw new Error('Failed to fetch readings');
    const data = await response.json();
    const valid = data.temperatureF !== null && data.humidity !== null;
    document.getElementById('temp').textContent = data.temperatureF === null ? '--' : data.temperatureF.toFixed(1) + ' \u00b0F';
    document.getElementById('humid').textContent = data.humidity === null ? '--' : data.humidity.toFixed(1) + ' %';
    document.getElementById('status').textContent = valid ? 'Live readings | ESP32 uptime: ' + data.uptimeSeconds + ' seconds' : 'DHT11 reading unavailable';
    document.getElementById('logState').textContent = data.logFull ? 'CSV is full: download your log' : (data.logging ? 'CSV logging active: one reading per 60 seconds' : 'CSV storage unavailable');
    if (valid) {
      points.push({uptime: data.uptimeSeconds, temperatureF: data.temperatureF, humidity: data.humidity});
      if (points.length > MAX_POINTS) points.shift();
      redraw();
    }
  } catch(e) {
    document.getElementById('status').textContent = 'Connection to ESP32 lost';
  }
}

redraw();
update();
setInterval(update, 3000);
window.addEventListener('resize', redraw);
</script>
</body>
</html>
)rawliteral";

void lcdLine(uint8_t row, String value) {
  while (value.length() < 16) value += ' ';
  lcd.setCursor(0, row);
  lcd.print(value.substring(0, 16));
}

void updateLCD() {
  if (isnan(temperatureF) || isnan(humidity)) {
    lcdLine(0, "DHT11 read error");
    lcdLine(1, "Check sensor");
  } else {
    lcdLine(0, "Temp: " + String(temperatureF, 1) + " F");
    lcdLine(1, "Humidity: " + String(humidity, 1) + "%");
  }
}

bool appendToLog() {
  if (!fileSystemReady || isnan(temperatureF) || isnan(humidity) || logFull) return false;
  File file = LittleFS.open(LOG_PATH, FILE_APPEND);
  if (!file) {
    Serial.println("Could not open CSV log");
    return false;
  }
  if (file.size() >= MAX_LOG_BYTES) {
    logFull = true;
    file.close();
    Serial.println("Log full; download a backup of your CSV file.");
    return false;
  }
  file.print(millis() / 1000UL); // seconds since boot, not real clock time
  file.print(',');
  file.print(temperatureF, 1);
  file.print(',');
  file.println(humidity, 1);
  file.close();
  Serial.println("Saved a reading to /readings.csv");
  return true;
}

void sendReadings() {
  String json = "{\"temperatureF\":";
  json += isnan(temperatureF) ? "null" : String(temperatureF, 1);
  json += ",\"humidity\":";
  json += isnan(humidity) ? "null" : String(humidity, 1);
  json += ",\"uptimeSeconds\":";
  json += String(millis() / 1000UL);
  json += ",\"logging\":";
  json += fileSystemReady ? "true" : "false";
  json += ",\"logFull\":";
  json += logFull ? "true" : "false";
  json += "}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void downloadLog() {
  if (!fileSystemReady || !LittleFS.exists(LOG_PATH)) {
    server.send(404, "text/plain", "CSV log not available");
    return;
  }
  File file = LittleFS.open(LOG_PATH, FILE_READ);
  if (!file) {
    server.send(500, "text/plain", "Cannot open CSV file");
    return;
  }
  server.sendHeader("Content-Disposition", "attachment; filename=ClimateTrack_readings.csv");
  server.streamFile(file, "text/csv");
  file.close();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  dht.begin();
  lcdLine(0, "ClimateTrack");
  lcdLine(1, "Starting...");

  // false: never erase your log automatically when a filesystem mount fails
  fileSystemReady = LittleFS.begin(false);
  if (!fileSystemReady) {
    Serial.println("LittleFS unavailable: dashboard works, but CSV logging is disabled");
  } else if (!LittleFS.exists(LOG_PATH)) {
    File file = LittleFS.open(LOG_PATH, FILE_WRITE);
    if (file) {
      file.println("uptime_seconds,temperature_f,humidity_percent");
      file.close();
    } else {
      fileSystemReady = false;
      Serial.println("Could not create CSV log");
    }
  }

  server.on("/", HTTP_GET, []() {server.send_P(200, "text/html", DASHBOARD);});
  server.on("/readings", HTTP_GET, sendReadings);
  server.on("/download", HTTP_GET, downloadLog);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
  Serial.println("Connecting to Wi-Fi/hotspot...");
  Serial.println("LCD and logging will work even without Wi-Fi.");
}

void loop() {
  const unsigned long now = millis();
  if (now - lastReadingMs >= READ_INTERVAL_MS) {
    lastReadingMs = now;
    temperatureF = dht.readTemperature(true);
    humidity = dht.readHumidity();
    updateLCD();
    if (!isnan(temperatureF) && !isnan(humidity) && fileSystemReady &&
        (!firstLogWritten || now - lastLogMs >= LOG_INTERVAL_MS)) {
      if (appendToLog()) {
        firstLogWritten = true;
        lastLogMs = now;
      }
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!serverStarted) {
      server.begin();
      serverStarted = true;
      Serial.println("Wi-Fi Connected!");
      Serial.print("Open dashboard: http://");
      Serial.println(WiFi.localIP());
    }
    server.handleClient();
  }
  delay(5);
}
