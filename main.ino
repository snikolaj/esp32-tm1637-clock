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
  
  // Show last 4 digits of IP address
  String ip = WiFi.localIP().toString();
  String lastFour = ip.substring(ip.lastIndexOf(".") + 1);
  while (lastFour.length() < 4) {
    lastFour = "0" + lastFour;  // Pad with leading zeros if needed
  }
  display.displayInt(lastFour.toInt());
  delay(3000);  // Show IP for 3 seconds
  
  // Initialize NTP
  timeClient.begin();
  timeClient.setTimeOffset(3600); // Adjust offset based on your timezone (3600 = +1 hour)
  
  // Setup web server routes
  setupWebServer();
  server.begin();
}

void loop() {
  server.handleClient();
  timeClient.update();
  
  // Update clock display
  int hours = timeClient.getHours();
  int minutes = timeClient.getMinutes();
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
