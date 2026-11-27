// ClimateTrack: ESP32 + DHT11 + I2C 16x2 LCD + Wi-Fi dashboard + CSV logging
// Board: ESP32 Dev Module (or your actual ESP32 board)


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
const unsigned long RETRY_WIFI_MS = 30000;
const size_t MAX_LOG_BYTES = 150000;  // Stops logging when this limit is reached.
const char* LOG_PATH = "/readings.csv";

float temperatureF = NAN;
float humidity = NAN;
unsigned long lastReadingMs = 0;
unsigned long lastLogMs = 0;
unsigned long lastWiFiRetryMs = 0;
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
<title>EnviroSense</title>
<style>
  *{box-sizing:border-box} body{margin:0;padding:30px 15px;background:#eef3f9;color:#17314b;font-family:Arial,sans-serif;text-align:center}
  main{max-width:440px;margin:auto}h1{margin:6px 0 3px}p{color:#62748a}
  .card{background:#fff;border-radius:16px;padding:25px;margin:14px 0;box-shadow:0 3px 12px #0001}
  .label{color:#62748a;font-size:16px}.value{font-size:39px;font-weight:bold;margin:8px 0 0}
  .foot{font-size:14px;color:#61758a;margin:15px 0}a{display:inline-block;background:#1268bd;color:#fff;text-decoration:none;padding:13px 19px;border-radius:10px;font-weight:bold}
  .small{font-size:13px}
</style>
</head>
<body><main>
  <h1>EnviroSense</h1>
  <p>ESP32 Temperature &amp; Humidity Monitor</p>
  <div class="card"><div class="label">Temperature</div><div id="temp" class="value">--</div></div>
  <div class="card"><div class="label">Humidity</div><div id="humid" class="value">--</div></div>
  <div id="status" class="foot">Connecting...</div>
  <a href="/download">Download CSV log</a>
  <p id="logState" class="small">Checking data logging...</p>
  <p class="small">Dashboard refreshes every 3 seconds. Log saved on ESP32.</p>
</main>
<script>
async function update() {
  try {
    const response = await fetch('/readings', {cache:'no-store'});
    if (!response.ok) throw new Error('request failed');
    const data = await response.json();
    document.getElementById('temp').textContent = data.temperatureF === null ? '--' : data.temperatureF.toFixed(1) + ' °F';
    document.getElementById('humid').textContent = data.humidity === null ? '--' : data.humidity.toFixed(1) + ' %';
    document.getElementById('status').textContent = data.temperatureF === null ? 'DHT11 reading unavailable' : 'Live readings · ESP32 uptime: ' + data.uptimeSeconds + ' seconds';
    document.getElementById('logState').textContent = data.logFull ? 'Log is full: download the CSV before creating a new log' : (data.logging ? 'CSV logging enabled (60-second intervals)' : 'CSV storage unavailable');
  } catch (e) {
    document.getElementById('status').textContent = 'ESP32 connection lost';
  }
}
update();
setInterval(update, 3000);
</script>
</body></html>
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
    Serial.println("Log is full. Download your CSV to save it.");
    return false;
  }
  // These timestamps are seconds since ESP32 boot, NOT real dates/times.
  file.print(millis() / 1000UL);
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
  server.sendHeader("Content-Disposition", "attachment; filename=EnviroSense_readings.csv");
  server.streamFile(file, "text/csv");
  file.close();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  dht.begin();
  lcdLine(0, "EnviroSense");
  lcdLine(1, "Starting...");

  // May format the filesystem if mount fails. Back up logs regularly.
  fileSystemReady = LittleFS.begin(true);
  if (!fileSystemReady) {
    Serial.println("LittleFS unavailable: dashboard will work without logging");
  } else if (!LittleFS.exists(LOG_PATH)) {
    File file = LittleFS.open(LOG_PATH, FILE_WRITE);
    if (file) {
      file.println("uptime_seconds,temperature_f,humidity_percent");
      file.close();
    } else {
      fileSystemReady = false;
      Serial.println("Could not create CSV file");
    }
  }

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", DASHBOARD);
  });
  server.on("/readings", HTTP_GET, sendReadings);
  server.on("/download", HTTP_GET, downloadLog);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
  lastWiFiRetryMs = millis();
  Serial.println("Connecting to Wi-Fi/hotspot (2.4 GHz)...");
  Serial.println("LCD and logging will work even without Wi-Fi.");
}

void loop() {
  unsigned long now = millis();

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
      Serial.print("Open dashboard: http://");
      Serial.println(WiFi.localIP());
    }
    server.handleClient();
  } else if (now - lastWiFiRetryMs >= RETRY_WIFI_MS) {
    lastWiFiRetryMs = now;
    Serial.println("Retrying Wi-Fi connection...");
    WiFi.reconnect();
  }
  delay(5);
}
