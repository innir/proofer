#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>
#include <TZ.h>
#include <ctime>
#include <tempProbe.h>

// No mDNS for OTA updates
#define NO_GLOBAL_MDNS

// Pin to control the relay is D1
#define RELAY_PIN D1

// Set your time zone, see TZ.h for available time zones
#define TZ TZ_Europe_Berlin

// Control heating every 10 seconds
#define UPDATE_INTERVAL_MS 10000

// Set range for upper/lower allowance
#define UPPER_ALLOWANCE 0
#define LOWER_ALLOWANCE 0.5

// Update csv once a minute
#define CSV_UPDATE_INTERVAL_MS 60000

float sTemp = 21.0;
float cTemp = 0.0;
bool isSystemOn = false;
bool isHeating = false;
bool prevHeatingState = false;
std::tm proofEnd = {};

uint64 previousMillis = 0;
uint64 csv_previousMillis = 0;

File csv;

AsyncWebServer server(80);

String getTime() {
  time_t now = std::time({});
  char timeString[std::size("yyyy-mm-dd hh:mm:ss")];
  std::strftime(std::data(timeString), std::size(timeString), "%F %T",
                std::localtime(&now));
  return String(timeString);
}

void writeCSV() {
  String text = getTime() + "," + String(cTemp) + "," +
                (isSystemOn ? String((int)isHeating) : "");
  csv.println(text);
  csv.flush();
}

void handleSetpoint(AsyncWebServerRequest *request) {
  if (request->method() == HTTP_POST) {
    if (!request->hasParam("value")) {
      request->send(400, "text/plain", "Invalid Request");
      return;
    }
    sTemp = (request->getParam("value")->value()).toFloat();
    request->send(200, "text/plain",
                  "OK: Temperature set to " + String(sTemp) + "°C");
  } else if (request->method() == HTTP_GET) {
    request->send(200, "text/plain", String(sTemp));
  }
}

void handleSytemState(AsyncWebServerRequest *request) {
  if (request->method() == HTTP_POST) {
    if (!request->hasParam("value")) {
      request->send(400, "text/plain", "Bad Request");
      return;
    }
    String val = request->getParam("value")->value();
    if (val.equalsIgnoreCase("on")) {
      isSystemOn = true;
    } else if (val.equalsIgnoreCase("off")) {
      isSystemOn = false;
    } else {
      request->send(400, "text/plain", "Bad Request");
      return;
    }
    request->send(200, "text/plain",
                  "OK: System is switched " +
                      String(isSystemOn ? "on" : "off"));
  } else if (request->method() == HTTP_GET) {
    request->send(200, "text/plain", String(isSystemOn ? "on" : "off"));
  }
}

void handleProofEnd(AsyncWebServerRequest *request) {
  if (request->method() == HTTP_POST) {
    if (!request->hasParam("value")) {
      request->send(400, "text/plain", "Bad Request");
      return;
    }
    char val[std::size("yyyy-mm-ddThh:mm:ss")];
    request->getParam("value")->value().toCharArray(val, std::size(val));
    strptime(val, "%FT%T", &proofEnd);
    request->send(200, "text/plain", "OK: Proofing end set");
  } else if (request->method() == HTTP_GET) {
    char timeString[std::size("yyyy-mm-ddThh:mm:ss")];
    std::strftime(std::data(timeString), std::size(timeString), "%FT%T",
                  &proofEnd);
    request->send(200, "text/plain", String(timeString));
  }
}

void setup(void) {
  Serial.begin(115200);

  // Initialize LittleFS
  if (!LittleFS.begin()) {
    Serial.println("Could not initialize LittleFS");
    return;
  }

  // Configure pin to control the relay
  pinMode(RELAY_PIN, OUTPUT);

  // Get WiFi credentials
  File cred = LittleFS.open("/cred.conf", "r");
  String ssid = cred.readStringUntil('\n');
  String password = cred.readStringUntil('\n');
  cred.close();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi...");
    delay(5000);
  }

  // Print IP address
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Start NTP server
  configTime(TZ, "pool.ntp.org");
  yield();
  delay(2000);
  yield();

  // Set up OTA
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {
      type = "filesystem";
      // Unmount FS before updating
      LittleFS.end();
    }
    Serial.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() { Serial.println("OTA update finished"); });

  ArduinoOTA.onError([](ota_error_t error) {
    (void)error;
    Serial.println("OTA update failed");
  });

  ArduinoOTA.begin();

  // Set up temp probe
  setupTempProbe();

  // Read temperature
  cTemp = getTemp();

  // Open log file
  csv = LittleFS.open("/data.csv", "w");

  // Write csv header and first log
  csv.println("Time,Temp,Heater");
  writeCSV();

  // Handle API calls
  server.on("/systemState", handleSytemState);
  server.on("/setpoint", handleSetpoint);
  server.on("/proofEnd", handleProofEnd);

  // Serve static files
  server.serveStatic("/data.csv", LittleFS, "/data.csv")
      .setCacheControl("max-age=0");
  server.serveStatic("/", LittleFS, "/www/")
      .setCacheControl("max-age=86400")
      .setDefaultFile("index.html");

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not Found");
  });

  // Set proofing end to now
  time_t now = std::time({});
  proofEnd = *std::localtime(&now);

  server.begin();
}

void loop(void) {
  ArduinoOTA.handle();

  uint64 currentMillis = millis();
  if (currentMillis - csv_previousMillis >= CSV_UPDATE_INTERVAL_MS) {
    csv_previousMillis = currentMillis;
    cTemp = getTemp();
    writeCSV();
    Serial.println(
        getTime() + ": System is " + String(isSystemOn ? "on" : "off") +
        ", heater is " + String(isHeating ? "on" : "off") + ", temp is " +
        String(cTemp) + "°C" + " and setpoint is " + String(sTemp) + "°C");
  }

  currentMillis = millis();
  if (currentMillis - previousMillis >= UPDATE_INTERVAL_MS) {
    previousMillis = currentMillis;
    prevHeatingState = isHeating;
    if (std::mktime(&proofEnd) <= std::time({})) {
      isSystemOn = false;
    }
    if (isSystemOn) {
      cTemp = getTemp();
      if (cTemp <= sTemp - LOWER_ALLOWANCE) {
        isHeating = true;
      } else if (cTemp >= sTemp + UPPER_ALLOWANCE) {
        isHeating = false;
      }
    } else {
      isHeating = false;
    }
    if (prevHeatingState != isHeating) {
      digitalWrite(RELAY_PIN, isHeating);
      writeCSV();
    }
  }
}
