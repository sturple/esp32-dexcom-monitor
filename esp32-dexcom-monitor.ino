// #define OTA_ENABLE
#define SERVER_ENABLE
#define DEXCOM_ENABLE

#include <WiFi.h>
#ifdef DEXCOM_ENABLE
#include <HTTPClient.h>
#include <ArduinoJson.h>
#endif
#ifdef SERVER_ENABLE
#include <WebServer.h>
#endif
#ifdef OTA_ENABLE
#include <ArduinoOTA.h>
#endif
#include <ESPmDNS.h>
#include <time.h>
#include <Preferences.h>


Preferences preferences;  // Create an NVS storage object

const char* dexcom_server = "https://shareous1.dexcom.com/ShareWebServices/Services/";
const float MMOL_CON = 0.0555;
const String MMOL_UNIT = "mmol/L";
const String MG_UNIT = "mg/dL";
int glucoseValue = 0;
char* glucoseTrend = "";
String glucoseTime = "";
String dexcom_session_id = "";
volatile bool fetchGlucoseFlag = true;

// Timer variables
int timer = 0;
int led_builtin = LED_BUILTIN;
// Variable to store the last toggle time
unsigned long previousMillis = 0;
// Interval for LED blinking (500ms)
unsigned long interval = 500;
String iconColor = "green";
// Variable to track LED state
int ledValue = 128;
int ledBrightness = 128;


#ifdef SERVER_ENABLE
WebServer server(80);
#endif


// The setup function
void setup() {
  // ******* Setup IO *********
  pinMode(led_builtin, OUTPUT);
  // ******* Setup NVS storage object *********
  preferences.begin("glucose_app", false);
  // ******* Setup Serial Port *********
  Serial.begin(115200);
  while (!Serial) delay(1000);
  setupWifi();

// ********* OTA Setup **********
#ifdef OTA_ENABLE
  setupOTA();
#endif

// ********* Web Server Setup **********
#ifdef SERVER_ENABLE
  // Server Routes
  server.on("/", handleRoot);
  server.on("/favicon.ico", HTTP_GET, handleFavicon);
  server.begin();
#endif
#ifdef DEXCOM_ENABLE
  setupDexcom();
#endif
  showMenu();
}


// The loop function
void loop() {
#ifdef DEXCOM_ENABLE
  if (fetchGlucoseFlag) {      // Check if the timer has triggered
    fetchGlucoseFlag = false;  // Reset flag
    getGlucoseReading();       // Fetch new glucose value
    if (glucoseValue > 0) {
      Serial.println(glucoseTime + " " + getGlucoseValue(glucoseValue) + " " + preferences.getString("dexcom_unit") + " " + glucoseTrend);
    }
  }

  delay(100);
  ++timer;
  // 100 = 1 second with a 100 delay
  if (timer > 3000) {
    onTimer();
    timer = 0;
  }
  setDexcomStatus();
#endif
#ifdef SERVER_ENABLE
  server.handleClient();
#endif
#ifdef OTA_ENABLE
  ArduinoOTA.handle();
#endif
  readInput();
}

// *************************** Setup Wifi *********************************
void setupWifi() {
  // Load stored Wi-Fi credentials
  String ssid = preferences.getString("ssid", "");
  String password = preferences.getString("wifi_password", "");
  int errorCount = 0;
  // ******* Setup WIFI *********
  Serial.println();
  Serial.println(ssid);
  if (ssid.length() > 0 && password.length() > 0) {
    Serial.print("Connecting to ");
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
      ++errorCount;
      // This means that having problems with logging in wifi, might want to re-enter credentaials
      if (errorCount > 20) {
        storeWiFiCredentials();
      }
    }
    Serial.println("");
    Serial.println("WiFi connected, IP address: " + WiFi.localIP());

    // ******* Setup mDNS *********
    String mDNS = preferences.getString("mdns", "esp32");
    if (MDNS.begin(mDNS)) {
      Serial.println(mDNS + " started");
    }
  } else {
    storeWiFiCredentials();
  }
}


// *************************** Setup OTA *********************************
#ifdef OTA_ENABLE
void setupOTA() {
  // Load stored Wi-Fi credentials
  String host = preferences.getString("ota_host", "");
  String password = preferences.getString("ota_password", "");
  // OTA Setup
  if (host.length() > 0 && password.length() > 0) {
    ArduinoOTA.setHostname(host);      // Set OTA hostname
    ArduinoOTA.setPassword(password);  // Set OTA password (optional)
    ArduinoOTA.onStart([]() {
      Serial.println("OTA Update Started");
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("OTA Update Complete");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress * 100) / total);
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("OTA Error [%u]\n", error);
    });

    ArduinoOTA.begin();
    Serial.println("OTA Ready");
  } else {
    storeOTACredentials();
  }
}

void storeOTACredentials() {
  Serial.println("Enter OTA host:");
  String host = readSerialInput();
  Serial.println("Enter OTA Password:");
  String password = readSerialInput();

  if (host.length() > 0 && password.length() > 0) {
    preferences.putString("ota_host", host);
    preferences.putString("ota_password", password);
    Serial.println("OTA credentials saved!");
    setupOTA();
  } else {
    Serial.println("Invalid input. OTA credentials not saved.");
  }
}
#endif




void readInput() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command.equalsIgnoreCase("MENU") || command.equalsIgnoreCase("HELP") || command.equalsIgnoreCase("?") || command.equalsIgnoreCase("H")) {
      showMenu();
    } else if (command.equalsIgnoreCase("SET WIFI")) {
      storeWiFiCredentials();
    }
#ifdef DEXCOM_ENABLE
    else if (command.equalsIgnoreCase("SET DEXCOM")) {
      storeDexcomCredentials();
    } else if (command.equalsIgnoreCase("SET DEXCOM CONFIG")) {
      storeDexcomConfig();
    } else if (command.equalsIgnoreCase("SHOW DEXCOM")){
      showDexcomValue();
    }
#endif
#ifdef OTA_ENABLE
    else if (command.equalsIgnoreCase("SET OTA")) {
      storeOTACredentials();
    }
#endif
    else if (command.equalsIgnoreCase("RESET")) {
      resetAllCredentials();
    }
  }
}

void showMenu() {
  Serial.println("");
  Serial.println("**************** Menu *******************");
  Serial.println("HELP - Shows this menu (alias MENU, H, ?)");
  Serial.println("SET WIFI - Sets Wifi credentials");
#ifdef DEXCOM_ENABLE
  Serial.println("SET DEXCOM - Sets Dexcom credentials");
  Serial.println("SET DEXCOM CONFIG - Set Dexcom units, high, low, ...");
  Serial.println("SHOW DEXCOM - Shows Current values");
#endif
#ifdef OTA_ENABLE
  Serial.println("SET OTA - Sets the Over the air updates");
#endif
  Serial.println("RESET - resets all credentials");
}

void storeWiFiCredentials() {
  Serial.println("Enter mDNS Host:");
  String mdns = readSerialInput();
  Serial.println("Enter Wi-Fi SSID:");
  String ssid = readSerialInput();
  Serial.println("Enter Wi-Fi Password:");
  String password = readSerialInput();

  if (ssid.length() > 0 && password.length() > 0) {
    preferences.putString("ssid", ssid);
    preferences.putString("wifi_password", password);
    preferences.putString("mdns", mdns);
    Serial.println("Wi-Fi credentials saved!");
    setupWifi();
  } else {
    Serial.println("Invalid input. Wi-Fi credentials not saved.");
  }
}


void resetAllCredentials() {
  Serial.println("***** Reset credentials *****");
  Serial.println("Type in 'y' and hit enter to reset all credentials");
  String confirm = readSerialInput();
  if (confirm.equalsIgnoreCase("Y")) {
    Serial.println("Deleting WIFI credentials");
    preferences.putString("ssid", "");
    preferences.putString("wifi_password", "");
    preferences.putString("mdns", "esp32");
    Serial.println("Deleting Dexcom credentials");
    preferences.putString("dexcom_id", "");
    preferences.putString("dexcom_password", "");
    Serial.println("Deleting Over the air credentials");
    preferences.putString("ota_host", "");
    preferences.putString("ota_password", "");
    Serial.println("Reseting Dexcom preferences to mmol, low (4), and high (10)");
    preferences.putFloat("dexcom_high", 10);
    preferences.putFloat("dexcom_low", 4);
    preferences.putFloat("dexcom_coef", 1);
    preferences.putString("dexcom_unit", MG_UNIT);
  } else {
    Serial.println("Credentials was NOT reset");
  }
}

String readSerialInput() {
  while (!Serial.available()) {
    delay(100);
  }
  String input = Serial.readStringUntil('\n');
  input.trim();
  return input;
}



// Interrupt Service Routine (ISR) - Runs every 60 sec
void onTimer() {
  fetchGlucoseFlag = true;  // Set flag to fetch glucose data
}


// Function to extract timestamp from "Date(1741550756222)"
long long extractTimestamp(const char* dateStr) {
  String str = String(dateStr);
  int start = str.indexOf('(') + 1;
  int end = str.indexOf(')');
  String timestampStr = str.substring(start, end);
  // Convert the substring to char* and then to long long
  const char* timestampCStr = timestampStr.c_str();  // Convert String to const char*
  return atoll(timestampCStr);
  ;  // Return the timestamp
}
// Function to convert timestamp to readable Pacific Time
String formatTimestamp(long long timestamp) {
  time_t rawtime = timestamp / 1000;  // Convert milliseconds to seconds

  // Set the time zone to Pacific Time (PST/PDT)
  setenv("TZ", "PST8PDT", 1);                 // Set the time zone to Pacific Time (UTC-8, daylight savings adjusts automatically)
  tzset();                                    // Apply the time zone setting
  struct tm* timeinfo = localtime(&rawtime);  // Convert to local time (Pacific Time)
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
  return String(buffer);  // Return formatted date string
}

// Function to convert trend direction to trend arrow
char* getTrendArrow(String trend) {
  if (trend == "DoubleUp") {
    return "⇈";
  } else if (trend == "SingleUp") {
    return "↑";  // Rising
  } else if (trend == "FortyFiveUp") {
    return "↗";  // Slowly rising
  } else if (trend == "Flat") {
    return "→";  // Steady
  } else if (trend == "FortyFiveDown") {
    return "↘";  // Slowly falling
  } else if (trend == "SingleDown") {
    return "↓";  // Falling
  } else if (trend == "DoubleDown") {
    return "⇊";  // Rapidly falling
  } else if (trend == "NotComputable") {
    return "NC";  // Not computable'
  } else if (trend == "RateOutOfRange") {
    return "??";  // Out of range
  } else {
    return "?";  // Unknown
  }
}

void setDexcomStatus() {
  float high = preferences.getFloat("dexcom_high", 10);
  float low = preferences.getFloat("dexcom_low", 4);
  float glucose_value = getGlucoseValue(glucoseValue);
  if (glucose_value > high) {
    ledBrightness = 5;
    interval = 1000;
    iconColor = "orange";
    flash_builtin();
  } else if (glucose_value < low) {
    interval = 100;
    ledBrightness = 100;
    iconColor = "red";
    flash_builtin();
  } else {
    interval = 5000;
    ledBrightness = 1;
    iconColor = "green";
    flash_builtin();
  }
}


void flash_builtin() {
  unsigned long currentMillis = millis();
  // Check if the interval has passed
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;      // Reset timer
    analogWrite(led_builtin, ledValue);  // Update LED
    if (ledValue > 0) {
      ledValue = 0;
    } else {
      ledValue = ledBrightness;
    }
  }
}

#ifdef DEXCOM_ENABLE

float getGlucoseValue(float value) {
  return value * preferences.getFloat("dexcom_coef", 1);
}

void showDexcomValue() {
  getGlucoseReading();
  Serial.println(glucoseTime + " " + getGlucoseValue(glucoseValue) + " " + preferences.getString("dexcom_unit") + " " + glucoseTrend);
}
// Function to get Dexcom session ID
bool setupDexcom() {
  String account_id = preferences.getString("dexcom_id", "");
  String password = preferences.getString("dexcom_password", "");
  // OTA Setup
  if (account_id.length() > 0 && password.length() > 0) {
    HTTPClient http;
    http.begin(String(dexcom_server) + "General/LoginPublisherAccountById");
    http.addHeader("Content-Type", "application/json");

    String requestBody = "{\"accountId\":\"" + String(account_id) + "\", \"password\":\"" + String(password) + "\", \"applicationId\":\"d89443d2-327c-4a6f-89e5-496bbb0317db\"}";

    int httpResponseCode = http.POST(requestBody);
    if (httpResponseCode == 200) {
      dexcom_session_id = http.getString();
      Serial.println(dexcom_session_id);
      dexcom_session_id.replace("\"", "");  // Remove quotes from response
      Serial.println("Session ID: " + dexcom_session_id);
      http.end();
      return true;
    } else {
      Serial.println("Failed to get session ID, HTTP Response: " + String(httpResponseCode));
      http.end();
      return false;
    }
  } else {
    storeDexcomCredentials();
  }
}

// Function to get glucose data
void getGlucoseReading() {
  if (dexcom_session_id == "") {
    Serial.println("No session ID, re-authenticating...");
    if (!setupDexcom()) return;
  }

  HTTPClient http;
  http.begin(String(dexcom_server) + "Publisher/ReadPublisherLatestGlucoseValues?sessionId=" + dexcom_session_id + "&minutes=1440&maxCount=1");
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.GET();
  if (httpResponseCode == 200) {
    String response = http.getString();
    // Parse JSON
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    if (doc.size() > 0) {
      glucoseValue = doc[0]["Value"];
      glucoseTrend = getTrendArrow(doc[0]["Trend"]);
      glucoseTime = formatTimestamp(extractTimestamp(doc[0]["WT"]));
    }
  } else {
    Serial.println("Failed to get glucose reading, HTTP Response: " + String(httpResponseCode));
    dexcom_session_id = "";
  }
  http.end();
}


void storeDexcomCredentials() {
  Serial.println("Enter Dexcom account ID:");
  String id = readSerialInput();
  Serial.println("Enter Dexcom Password:");
  String password = readSerialInput();

  if (id.length() > 0 && password.length() > 0) {
    preferences.putString("dexcom_id", id);
    preferences.putString("dexcom_password", password);
    Serial.println("Dexcom credentials saved!");
    setupDexcom();
  } else {
    Serial.println("Invalid input. Dexcom credentials not saved.");
  }
}

void storeDexcomConfig() {
  Serial.println("Choose Units  (1) for " + MMOL_UNIT + ", or enter for " + MG_UNIT);
  String units = readSerialInput();
  Serial.println("Choose High Alarm");
  float high = readSerialInput().toFloat();
  Serial.println("Choose Low Alarm");
  float low = readSerialInput().toFloat();

  if (units == "1") {
    preferences.putString("dexcom_unit", MMOL_UNIT);
    preferences.putFloat("dexcom_coef", MMOL_CON);
    Serial.println("should be mmol");
  } else {
    preferences.putString("dexcom_unit", MG_UNIT);
    preferences.putFloat("dexcom_coef", 1);
    Serial.println("should be mg");
  }
  preferences.putFloat("dexcom_high", high);
  preferences.putFloat("dexcom_low", low);


  Serial.println("Units: " + preferences.getString("dexcom_unit") + " coef: " + String(preferences.getFloat("dexcom_coef"), 2));
  Serial.println("High: " + String(preferences.getFloat("dexcom_high"), 2) + " Low: " + String(preferences.getFloat("dexcom_low"), 2));
}
#endif



// *************************** Web Server *********************************
#ifdef SERVER_ENABLE
void handleRoot() {
  char temp[1000];
  int sec = millis() / 1000;
  int hr = sec / 3600;
  int min = (sec / 60) % 60;
  sec = sec % 60;

  char reading[50];  // Allocate enough space for the formatted string
  sprintf(reading, "%.2f %s %s", getGlucoseValue(glucoseValue), preferences.getString("dexcom_unit", "ERR"), glucoseTrend);


  snprintf(
    temp, 1000,
    "<html>\
  <head>\
    <meta http-equiv='refresh' content='30'/>\
    <meta charset='UTF-8'>\
    <link rel='icon' type='image/x-icon' href='/favicon.ico'> \
    <link rel='mask-icon' href='/favicon.ico'> \
    <title>%s</title>\
    <style>\
      body { \
        background-color: #dddddd; \
        font-family: Arial, Helvetica, Sans-Serif; \
        max-width: 800px; \
        margin: 1rem auto; \
        text-align: center; \
      }\
      h1 { margin-bottom: 0;} \
      main { padding: 1rem;} \
      .glucose-data { font-size: 2rem; padding: 1rem; } \
      .glucose-data span {font-weight: bold; padding-left: 2rem;} \
      .glucose-data p { font-size: 0.8rem;}\
    </style>\
  </head>\
  <body>\
    <main>\
    <div class='glucose-data'>\
    %s %s<p>%s</p>\
    </div> \
    </main> \
  </body>\
  </html>",
    reading, generateFaviconSVG().c_str(), reading, glucoseTime.c_str());
  server.send(200, "text/html", temp);
}

// Function to generate the SVG for favicon based on state
String generateFaviconSVG() {
  // SVG for a circle with dynamic color
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
         "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"32\" height=\"32\" viewBox=\"0 0 32 32\">"
         "<circle cx=\"16\" cy=\"16\" r=\"14\" fill=\""
         + iconColor + "\" />"
                       "</svg>";
}

// Serve the favicon
void handleFavicon() {
  String favicon = generateFaviconSVG();
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(200, "image/svg+xml", favicon);
}
#endif
