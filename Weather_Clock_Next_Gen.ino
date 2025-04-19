// CrowPanel ESP32 4.2" E-Paper Display with OpenWeatherMap One Call 3.0 Data
// Refactored

#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include "EPD.h"       // E-Paper display driver (SSD1683)
#include "pic.h"       // Icon bitmaps
#include <time.h>       // NTP and timezone functions

// ====== Configuration ======
static const char* WIFI_SSID     = "YOURWIFISSID";        // WiFi SSID
static const char* WIFI_PASS     = "YOURWIFIPASSWORD";           // WiFi password
static const char* NTP_SERVER    = "pool.ntp.org";       // NTP server
static const char* TZ_INFO       = "CST6CDT,M3.2.0/2,M11.1.0/2"; // US Central Look Here for yours --> https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv

// OpenWeatherMap One Call 3.0 parameters
static const String OWM_API_KEY  = "YOUROPENWEATHERMAPAPIKEY"; // API key
static const double LATITUDE     = 35.1495;   // Memphis, TN
static const double LONGITUDE    = -90.0490;  // Memphis, TN

// Refresh intervals
static const unsigned long WEATHER_UPDATE_INTERVAL = 30UL * 60UL * 1000UL; // 30 minutes
static const unsigned long SCREEN_REFRESH_INTERVAL  =  1UL * 60UL * 1000UL; // 1 minute

// Icon dimensions
#define TEMP_ICON_W 216
#define TEMP_ICON_H  64
#define HUMI_ICON_W 184
#define HUMI_ICON_H  88

// Display buffer
static uint8_t ImageBW[15000];

// Data trackers
unsigned long lastWeatherUpdate = 0;
unsigned long lastScreenRefresh = 0;
String currentTemperature;
String currentHumidity;
String forecastHour[3];

// Prototypes
void initSerial();
void initWiFi();
void syncTime();
void initDisplay();
void fetchWeatherData();
void updateDisplay();
String httpGETRequest(const char* url);

void setup() {
  initSerial();
  initWiFi();
  syncTime();
  initDisplay();

  fetchWeatherData();
  updateDisplay();
}

void loop() {
  unsigned long now = millis();
  if (now - lastScreenRefresh >= SCREEN_REFRESH_INTERVAL) {
    updateDisplay();
    lastScreenRefresh = now;
  }
  if (now - lastWeatherUpdate >= WEATHER_UPDATE_INTERVAL) {
    fetchWeatherData();
    lastWeatherUpdate = now;
  }
}

void initSerial() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Serial initialized.");
}

void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println(" connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void syncTime() {
  configTzTime(TZ_INFO, NTP_SERVER);
  Serial.print("Syncing time");
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    Serial.print('.');
    delay(500);
    now = time(nullptr);
  }
  struct tm tm; localtime_r(&now, &tm);
  Serial.print(" Time synced: ");
  Serial.print(asctime(&tm));
}

void initDisplay() {
  pinMode(7, OUTPUT); digitalWrite(7, HIGH);
  EPD_GPIOInit(); EPD_Init(); EPD_Clear(); EPD_Update();
  Paint_NewImage(ImageBW, EPD_W, EPD_H, 0, WHITE);
  Paint_Clear(WHITE); EPD_Display(ImageBW);
  EPD_Clear_R26H(ImageBW); EPD_Update();
}

void fetchWeatherData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected; skipping fetch.");
    return;
  }
  char url[256];
  snprintf(url, sizeof(url),
           "https://api.openweathermap.org/data/3.0/onecall?lat=%.4f&lon=%.4f"
           "&exclude=minutely,daily,alerts&units=imperial&appid=%s",
           LATITUDE, LONGITUDE, OWM_API_KEY.c_str());
  Serial.println("Requesting weather: "); Serial.println(url);
  String payload = httpGETRequest(url);
  Serial.println("Payload: " + payload);
  JSONVar root = JSON.parse(payload);
  if (JSON.typeof(root) == "undefined") {
    Serial.println("Failed to parse JSON.");
    return;
  }
  currentTemperature = JSON.stringify(root["current"]["temp"]);
  currentHumidity    = JSON.stringify(root["current"]["humidity"]);
  for (int i = 1; i <= 3; i++) {
    forecastHour[i-1] = JSON.stringify(root["hourly"][i]["temp"]);
  }
}

void updateDisplay() {
  time_t t = time(nullptr);
  struct tm* tmInfo = localtime(&t);
  char dateBuf[16], timeBuf[16], buf[32];

  // Format date/time
  strftime(dateBuf, sizeof(dateBuf), "%m-%d-%Y", tmInfo);
  strftime(timeBuf, sizeof(timeBuf), "%I:%M", tmInfo);

  // Date & Time (above line)
  EPD_ShowString(80, 0,   dateBuf, 48, BLACK);
  EPD_ShowString(80, 48,  timeBuf, 96, BLACK);

  // Icons (above line, below time)
  EPD_ShowPicture(0,  150, TEMP_ICON_W, TEMP_ICON_H, gImage_pic_temp1, WHITE);
  EPD_ShowPicture(216,140, HUMI_ICON_W, HUMI_ICON_H, gImage_pic_humi, WHITE);

  // Current weather (just below line)
  snprintf(buf, sizeof(buf), "%s F",  currentTemperature.c_str());
  EPD_ShowString(90, 200, buf, 24, BLACK);
  snprintf(buf, sizeof(buf), "%s%%", currentHumidity.c_str());
  EPD_ShowString(330,200, buf, 24, BLACK);

  // 3-hour forecast (further down)
  for (int i = 0; i < 3; i++) {
    int x = 10 + i * 130;
    snprintf(buf, sizeof(buf), "+%dh %sF", i + 1, forecastHour[i].c_str());
    EPD_ShowString(x, 250, buf, 24, BLACK);
  }

  EPD_Display(ImageBW);
  EPD_Update_Part();
  Serial.println("Display updated.");
}

String httpGETRequest(const char* url) {
  HTTPClient http; http.begin(url);
  int code = http.GET(); String r = "{}";
  if (code > 0) r = http.getString(); else { Serial.print("HTTP error: "); Serial.println(code); }
  http.end(); return r;
}
