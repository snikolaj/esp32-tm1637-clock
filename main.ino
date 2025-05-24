#include <WiFi.h>
#include <WebServer.h>
#include <GyverTM1637.h>
#include <Adafruit_NeoPixel.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Network credentials
const char* ssid = "<INSERT_SSID_HERE>";
const char* password = "<INSERT_PASSWORD_HERE>";

// Pin definitions
#define TM1637_CLK  2    // Define clock pin for TM1637
#define TM1637_DIO  3    // Define data pin for TM1637
#define LED_PIN     7    // WS2812B LED pin
#define NUM_LEDS    1    // Number of WS2812B LEDs

// Objects
GyverTM1637 display(TM1637_CLK, TM1637_DIO);
Adafruit_NeoPixel pixels(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Global variables
uint8_t brightness = 7;  // TM1637 brightness (0-7)
uint8_t r = 0, g = 0, b = 0;  // RGB values for WS2812B

// Time keeping variables
unsigned long lastSyncTime = 0;          // millis() when last synced with NTP
unsigned long syncInterval = 4 * 60 * 60 * 1000UL; // 4 hours in milliseconds
unsigned long baseTimeSeconds = 0;       // Base time in seconds since midnight

// Function to sync with NTP server
void syncTimeWithNTP() {
  Serial.print("Syncing with NTP server... ");
  
  // Set timezone for Germany (you'll need to manually adjust for DST)
  timeClient.setTimeOffset(7200); // 3600 for CET (winter time)
  // Change to 7200 for CEST (summer time) manually when needed
  
  timeClient.update();
  
  int hours = timeClient.getHours();
  int minutes = timeClient.getMinutes();
  int seconds = timeClient.getSeconds();
  
  // Store the current time as seconds since midnight
  baseTimeSeconds = hours * 3600UL + minutes * 60UL + seconds;
  lastSyncTime = millis();
  
  Serial.println("Done!");
  Serial.printf("Synced time: %02d:%02d:%02d\n", hours, minutes, seconds);
}

// Unused for now, will add later
bool isDST(int month, int day, int hour) {
  // DST runs from last Sunday in March to last Sunday in October
  if (month < 3 || month > 10) return false;
  if (month > 3 && month < 10) return true;
  
  // For March and October, we need more precise logic
  // This is a simplified version - you might want more accurate calculation
  if (month == 3) return day >= 25; // Approximate last Sunday
  if (month == 10) return day < 25;  // Approximate last Sunday
  
  return false;
}

// Function to get current time based on internal timekeeping
void getCurrentTime(int &hours, int &minutes, int &seconds) {
  unsigned long currentMillis = millis();
  
  // Handle millis() overflow (occurs every ~49.7 days)
  unsigned long elapsedMillis;
  if (currentMillis >= lastSyncTime) {
    elapsedMillis = currentMillis - lastSyncTime;
  } else {
    // Overflow occurred
    elapsedMillis = (0xFFFFFFFF - lastSyncTime) + currentMillis + 1;
  }
  
  unsigned long elapsedSeconds = elapsedMillis / 1000;
  unsigned long totalSeconds = (baseTimeSeconds + elapsedSeconds) % (24 * 3600UL); // Wrap at 24 hours
  
  hours = totalSeconds / 3600;
  minutes = (totalSeconds % 3600) / 60;
  seconds = totalSeconds % 60;
}

void setup() {
  Serial.begin(115200);
  
  // Initialize LED
  pixels.begin();
  pixels.show();
  
  // Initialize display
  display.clear();
  display.brightness(brightness);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  
  // Initialize NTP and do initial sync
  timeClient.begin();
  timeClient.setTimeOffset(3600); // Adjust offset based on your timezone (3600 = +1 hour)
  syncTimeWithNTP(); // Initial sync
  
  // Show last 4 digits of IP address
  String ip = WiFi.localIP().toString();
  String lastFour = ip.substring(ip.lastIndexOf(".") + 1);
  while (lastFour.length() < 4) {
    lastFour = "0" + lastFour;  // Pad with leading zeros if needed
  }
  display.displayInt(lastFour.toInt());
  delay(3000);  // Show IP for 3 seconds
  
  // Setup web server routes
  setupWebServer();
  server.begin();
}

void loop() {
  server.handleClient();
  
  // Check if we need to sync with NTP (every 4 hours)
  unsigned long currentMillis = millis();
  bool needSync = false;
  
  if (currentMillis >= lastSyncTime) {
    needSync = (currentMillis - lastSyncTime) >= syncInterval;
  } else {
    // Handle millis() overflow case
    needSync = ((0xFFFFFFFF - lastSyncTime) + currentMillis + 1) >= syncInterval;
  }
  
  if (needSync) {
    syncTimeWithNTP();
  }
  
  // Get current time using internal timekeeping
  int hours, minutes, seconds;
  getCurrentTime(hours, minutes, seconds);
  
  // Update clock display
  display.point(true);  // Turn on the colon
  display.displayClock(hours, minutes);
  
  // Update LED color
  pixels.setPixelColor(0, pixels.Color(r, g, b));
  pixels.show();
  
  delay(1000);
}

void setupWebServer() {
  // Serve the main page
  server.on("/", HTTP_GET, []() {
    String html = "<html><body>"
                 "<h1>ESP32 Clock Control</h1>"
                 "<h2>LED Color</h2>"
                 "<form action='/color' method='get'>"
                 "Red (0-255): <input type='number' name='r' value='" + String(r) + "' min='0' max='255'><br>"
                 "Green (0-255): <input type='number' name='g' value='" + String(g) + "' min='0' max='255'><br>"
                 "Blue (0-255): <input type='number' name='b' value='" + String(b) + "' min='0' max='255'><br>"
                 "<input type='submit' value='Set Color'>"
                 "</form>"
                 "<h2>Display Brightness</h2>"
                 "<form action='/brightness' method='get'>"
                 "Brightness (0-7): <input type='number' name='bright' value='" + String(brightness) + "' min='0' max='7'><br>"
                 "<input type='submit' value='Set Brightness'>"
                 "</form></body></html>";
    server.send(200, "text/html", html);
  });

  // Handle color updates
  server.on("/color", HTTP_GET, []() {
    r = server.arg("r").toInt();
    g = server.arg("g").toInt();
    b = server.arg("b").toInt();
    server.sendHeader("Location", "/");
    server.send(303);
  });

  // Handle brightness updates
  server.on("/brightness", HTTP_GET, []() {
    brightness = server.arg("bright").toInt();
    display.brightness(brightness);
    server.sendHeader("Location", "/");
    server.send(303);
  });
}
