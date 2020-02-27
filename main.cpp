#include <Wire.h>
#include "SSD1306Wire.h"
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <esp8266wifi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <esp8266httpclient.h>
#include <Time.h>

/*
 * A simple OctoPrint-Monitor for ESP8266 and SSD1306 OLED
 * Feb. 2020, Stefan Onderka, https://github.com/noppingen
*/

// Misc
int splash_delay = 3;  // Delay after splash (seconds)
int setup_delay  = 2;  // Delay after setup (seconds)
int loop_delay   = 30; // Interval to query printer (seconds)
char tempstring[10];
bool STATE_PRINTING;
bool STATE_ERROR;
bool STATE_OFFLINE;
bool STATE_OPERATIONAL;
static uint16_t days;
static uint8_t  hours, minutes, seconds;

// OctoPrint
const char* HTTP_URL_JOB      = "http://YOUR_OCTOPRINT_HOST/api/job";
const char* HTTP_URL_PRINTER  = "http://YOUR_OCTOPRINT_HOST/api/printer";
const char* OCTOPRINT_API_KEY = "YOUR_OCTOPRINT_API_KEY";

// WiFi and NTP
const char* WIFI_SSID  = "YOUR_WIFI_SSID";
const char* WIFI_PASS  = "YOUR_WIFI_PASS";
const char* NTP_SERVER = "YOUR_NTP_HOST";
WiFiClient espClient;

// I2C
#define SCL_PIN 9  // GPIO 9
#define SDA_PIN 10 // GPIO 10

// NTP
time_t TIMESTAMP;
int NTP_OFFSET    =          0; // Time in UTC
int MIN_TIMESTAMP = 1581608492; // Februar 13th, 2020, to check NTP validity
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER, NTP_OFFSET);

// SSD1306 OLED
#define OLED_I2C_ADDRESS 0x3c
SSD1306Wire display(OLED_I2C_ADDRESS, SDA_PIN, SCL_PIN);
int OLED_LINE_HEIGHT = 13; // Line height with font size 10
int H_OLED_CENTER    = 64; // Horizontal center of OLED

// HTTP client
HTTPClient http;

// JSON
DynamicJsonDocument json_printer(1024); // Arbitrary set to 1024b
DynamicJsonDocument json_job(1024);     // Arbitrary set to 1024b
String json_payload_printer = "";
String json_payload_job = "";

// Function: Seconds to hh:mm:ss
String seconds_to_string( long unsigned int secs ) {
  // Did I mention I hate C/C++?
  seconds = secs % (uint32_t)60;
  secs /= (uint32_t)60;
  minutes = secs % (uint32_t)60;
  secs /= (uint32_t)60;
  hours = secs % (uint32_t)24;
  days = secs / (uint32_t)32;
  sprintf(tempstring, "%02d", hours);
  String p_hours = (String)tempstring;
  sprintf(tempstring, "%02d", minutes);
  String p_minutes = (String)tempstring;
  sprintf(tempstring, "%02d", seconds);
  String p_seconds = (String)tempstring;
  if (days > 0) {
    String p_days = (String)days;
    return (String)p_days + "d" + p_hours + ":" + p_minutes + ":" + p_seconds;
  } else {
    return (String)p_hours + ":" + p_minutes + ":" + p_seconds;
  }
}

// Setup
void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  delay(500);

  Serial.println();
  Serial.println();
  Serial.println(F("OctoPrint-Monitor"));
  Serial.println(F("-----------------"));

  // OLED
  Serial.println(F("OLED: Init"));
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.clear();

  display.setContrast(100);
  display.drawString(H_OLED_CENTER, 30, "OctoPrint Monitor");
  display.display();
  delay(splash_delay * 1000);

  // WiFi
  display.clear();

  display.drawString(H_OLED_CENTER, 30, "WiFi");
  display.display();

  Serial.print(F("WiFi: Connecting to SSID "));
  Serial.print(WIFI_SSID);
 
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  Serial.print(" .");
  }

  Serial.println();
  Serial.println(F("WiFi: Connected"));
  Serial.print(F("* IP address:           "));
  Serial.println(WiFi.localIP());
  Serial.print(F("* MAC address:          "));
  Serial.println(WiFi.macAddress());
  delay(2000);

  // NTP
  // ------------------------------------------
  display.clear();

  display.drawString(H_OLED_CENTER, 30, "NTP");
  display.display();

  Serial.println(F("NTP: Init"));
  Serial.println(F("* Time sync"));
  timeClient.begin();
  Serial.println(F("* Waiting 2 seconds"));
  delay(2000);
  timeClient.update();
  delay(2000);
  TIMESTAMP = timeClient.getEpochTime();
  if ( TIMESTAMP < MIN_TIMESTAMP) {
    // Timestamp not valid
    do {
      TIMESTAMP = timeClient.getEpochTime();
      Serial.print(F("NTP... "));
      delay(1000);
    } while (TIMESTAMP < MIN_TIMESTAMP);
    Serial.println();
  }
  TIMESTAMP = timeClient.getEpochTime();
  Serial.print("* Time (UTC):           ");
  Serial.println(timeClient.getFormattedTime());
  Serial.print("* Unix Timestamp:       ");
  Serial.println(TIMESTAMP);

  display.clear();

  display.drawString(H_OLED_CENTER, 30, "Connected");
  display.display();
  delay(setup_delay * 1000);
  display.clear();
}

// Loop
void loop() {
  // HTTP: Printer status
  Serial.println("HTTP: Printer status");
  http.begin(espClient, HTTP_URL_PRINTER);
  http.setUserAgent("ESP8266HTTPClient-OctoPrint-Monitor");
  http.addHeader("X-Api-Key", OCTOPRINT_API_KEY);
  http.setTimeout(1000);

  int http_code_get_printer = http.GET();
  if (http_code_get_printer == 200) {
    // HTTP OK
    Serial.println("* HTTP OK");
    json_payload_printer = http.getString();
    // Show JSON
    //Serial.println(json_payload_printer);
    //Serial.println();
  } else {
    // HTTP failed
    Serial.println("* HTTP failed");
    Serial.print("* HTTP code: ");
    Serial.println(http_code_get_printer);
    json_payload_printer = "{}";
  }
  http.end();

  // HTTP: Job status
  Serial.println("HTTP: Job status");
  http.begin(espClient, HTTP_URL_JOB);
  http.setUserAgent("ESP8266HTTPClient-OctoPrint-Display");
  http.addHeader("X-Api-Key", OCTOPRINT_API_KEY);
  http.setTimeout(1000);

  int http_code_get_job = http.GET();
  if (http_code_get_job == 200) {
    // HTTP OK
    Serial.println("* HTTP OK");
    json_payload_job = http.getString();
    // Show JSON
    //Serial.println(json_payload_job);
    //Serial.println();
  } else {
    // HTTP failed
    Serial.println("* HTTP Failed");
    Serial.print("* HTTP code: ");
    Serial.println(http_code_get_job);
    json_payload_job = "{}";
  }
  http.end();
  
  if ( (http_code_get_printer != - 1) && ( http_code_get_job != -1) ) {
    // Both API calls OK, parse JSON objects
    Serial.println(F("JSON: Deserialization"));
    DeserializationError error_printer = deserializeJson(json_printer, json_payload_printer);
    DeserializationError error_job     = deserializeJson(json_job,     json_payload_job);

    if (error_printer || error_job) {
      // JSON deserialization error(s)
      Serial.print(F("* JSON: Error (printer): "));
      Serial.println(error_printer.c_str());
      Serial.print(F("* JSON: Error (job):     "));
      Serial.println(error_job.c_str());
      return;
    } else {
      // Extract data from JSON
      Serial.println(F("JSON: Extracting data"));
      Serial.println();

      // Printer status
      String state_string      = json_printer["state"]["text"];
      String flag_state        = json_printer["state"]["flags"]["ready"];
      String flag_error        = json_printer["state"]["flags"]["error"];
      String flag_printing     = json_printer["state"]["flags"]["printing"];
      float t_bed_actual       = json_printer["temperature"]["bed"]["actual"];
      float t_bed_target       = json_printer["temperature"]["bed"]["target"];
      float t_tool0_actual     = json_printer["temperature"]["tool0"]["actual"];
      float t_tool0_target     = json_printer["temperature"]["tool0"]["target"];

      // Job status
      String job_state         = json_job["state"];
      String file_name         = json_job["job"]["file"]["name"];
      int est_print_time       = json_job["job"]["estimatedPrintTime"];
      float print_progress     = json_job["progress"]["completion"];
      int time_printing        = json_job["progress"]["printTime"];
      int time_remaining       = json_job["progress"]["printTimeLeft"];

      // Various printer states
      if ( job_state == "Printing" ) {
        STATE_PRINTING = true;
        STATE_ERROR = false;
        STATE_OPERATIONAL = true;
        STATE_OFFLINE = false;
      }

      if ( job_state == "Error" ) {
        STATE_PRINTING = false;
        STATE_ERROR = true;
        STATE_OPERATIONAL = false;
        STATE_OFFLINE = false;
      }

      if ( job_state == "Offline" ) {
        STATE_PRINTING = false;
        STATE_ERROR = false;
        STATE_OPERATIONAL = true;
        STATE_OFFLINE = true;
      }

      if ( job_state == "Operational" ) {
        STATE_PRINTING = false;
        STATE_ERROR = false;
        STATE_OPERATIONAL = true;
        STATE_OFFLINE = false;
      }

      // Strip ".gcode" from filename
      int string_length = file_name.length();
      file_name.remove(string_length - 6);

      // Format progress percentage
      char print_progress_formatted[10];
      sprintf(print_progress_formatted, "%.2f", print_progress);

      // Format tBed temperature difference 
      float t_bed_diff         = (t_bed_target - t_bed_actual) * -1;
      char t_bed_diff_formatted[10];
      sprintf(t_bed_diff_formatted, "%.2f", t_bed_diff);

      // Format tTool0 temperature difference
      float t_tool0_diff       = (t_tool0_target - t_tool0_actual) * -1;
      char t_tool0_diff_formatted[10];
      sprintf(t_tool0_diff_formatted, "%.2f", t_tool0_diff);

      /*
      // Time stuff - further development is up to you
      time_t utcCalc = timestamp;
      time_t time_diff = timeClient.getEpochTime() - timestamp;
      Serial.print(F("Time          "));
      Serial.println(timeClient.getFormattedTime());
      Serial.println(F("--------------------------------"));
      */

      // Serial output
      Serial.print(F("Status:         "));
      Serial.println(state_string);
      Serial.print(F("Ready:          "));
      Serial.println(flag_state);
      Serial.print(F("Error:          "));
      Serial.println(STATE_ERROR);
      Serial.print(F("Bed actual:     "));
      Serial.println(t_bed_actual);
      Serial.print(F("Bed target:     "));
      Serial.println(t_bed_target);
      Serial.print(F("Bed diff:       "));
      Serial.println(t_bed_diff_formatted);
      Serial.print(F("Tool0 actual:   "));
      Serial.println(t_tool0_actual);
      Serial.print(F("Tool0 target:   "));
      Serial.println(t_tool0_target);
      Serial.print(F("Tool0 diff:     "));
      Serial.println(t_tool0_diff_formatted);
      Serial.print(F("Job state:      "));
      Serial.println(job_state);
      Serial.print(F("File name:      "));
      Serial.println(file_name);
      Serial.print(F("Progress:       "));
      Serial.println(print_progress_formatted);
      Serial.print(F("Estimated time: "));
      Serial.println(est_print_time);
      Serial.print(F("Time printing:  "));
      Serial.println(seconds_to_string(time_printing));
      Serial.print(F("Time remaining: "));
      Serial.println(seconds_to_string(time_remaining));
      Serial.println();

      /*
      Serial.print(F("HTTP code prnt: "));
      Serial.println(http_code_get_printer);
      Serial.print(F("HTTP code job:  "));
      Serial.println(http_code_get_job);
      */

      // OLED output
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.clear();
      Serial.println("OLED: Bright");
      // OLED bright
      display.setContrast(100);
      Serial.println("OLED: Update");

      // Line 1
      display.drawString(0,             (OLED_LINE_HEIGHT *0 ),  "Status:");
      display.drawString(H_OLED_CENTER, (OLED_LINE_HEIGHT *0 ), job_state);

      // Line 2
      display.drawString(0,             (OLED_LINE_HEIGHT * 1), "Progress:");
      display.drawString(H_OLED_CENTER, (OLED_LINE_HEIGHT * 1), (String)print_progress + " %");

      // Line 3
      display.drawString(0,             (OLED_LINE_HEIGHT * 2), "Elapsed:");
      display.drawString(H_OLED_CENTER, (OLED_LINE_HEIGHT * 2), seconds_to_string(time_printing));

      // Line 4
      display.drawString(0,             (OLED_LINE_HEIGHT * 3), "Remaining:");
      display.drawString(H_OLED_CENTER, (OLED_LINE_HEIGHT * 3), seconds_to_string(time_remaining));

      // Line 5
      display.drawString(0,             (OLED_LINE_HEIGHT * 4), "T:");
      display.drawString(13,            (OLED_LINE_HEIGHT * 4), (String)t_tool0_actual + " °C");
      display.drawString(H_OLED_CENTER, (OLED_LINE_HEIGHT * 4), "B:");
      display.drawString(77, (OLED_LINE_HEIGHT * 4), (String)t_bed_actual + " °C");

      // Update OLED
      display.display();
    }
  } else {
    // One or both API calls failed
    Serial.println();
    Serial.println("OLED: Dimmed");
    // OLED dimmed
    display.setContrast(30, 10, 5);

    display.setTextAlignment(TEXT_ALIGN_CENTER);
    Serial.println("OLED: Update");
    display.clear();

    display.drawString(H_OLED_CENTER, (OLED_LINE_HEIGHT * 1), "API error");
    display.drawString(H_OLED_CENTER, (OLED_LINE_HEIGHT * 2), "or");
    display.drawString(H_OLED_CENTER, (OLED_LINE_HEIGHT * 3), "OctoPrint offline");
    display.display();
    Serial.println();
  }

  delay(loop_delay * 1000);
}
