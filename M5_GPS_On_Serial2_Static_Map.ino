#include <ESP32-Chimera-Core.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>

#define ILI9341_DISPON  0x29
#define ILI9341_DISPOFF 0x28

HardwareSerial gpsSerial = HardwareSerial(2);
char buffer[256];
int count = 0;
bool hasFix = false, noFixSoFar = true, screenOn = true;;
float myLat, myLong;
double lastMapDraw = 0;
String myLatText, myLongText, fixTime, fixDate;
uint8_t zoom = 12; // Set up whatever initial zoom level you like
String COORDS = "";

void buttons_test() {
  // Button A: decrease zoom
  // Button C: increase zoom
  // Button B: switch display on/off

  bool needRedraw = false;
  while (M5.BtnA.isPressed()) {
    if (zoom > 0) zoom -= 1;
    Serial.println("-1 zoom: " + String(zoom));
    delay(200);
    M5.update();
    needRedraw = true;
  }
  while (M5.BtnC.isPressed()) {
    if (zoom < 20) zoom += 1;
    Serial.println("+1 zoom: " + String(zoom));
    delay(200);
    M5.update();
    needRedraw = true;
  }

  if (M5.BtnB.isPressed()) {
    while (M5.BtnB.isPressed()) M5.update();
    if (screenOn) {
      Serial.println("DISPOFF");
      M5.Lcd.writecommand(ILI9341_DISPOFF);
      M5.Lcd.setBrightness(0);
      screenOn = false;
    } else {
      Serial.println("DISPON");
      M5.Lcd.writecommand(ILI9341_DISPON);
      M5.Lcd.setBrightness(100);
      screenOn = true;
    }
  }
  if (needRedraw) drawMap();
}

void drawMap() {
  Serial.println("Displaying map at " + COORDS + " @ zoom " + String(zoom));
  HTTPClient http;
  Serial.print("[HTTP] begin...\n");
  // Using the MapQuest API. Get your own key!
https://www.mapquestapi.com/staticmap/v5/map?key=.............&center=22.459969,114.00457&size=1280,640&zoom=14&size=@2x&locations=22.469969,114.00|flag-lg-7b0099-ping||22.459969,114.00457|marker-start
  http.begin("https://www.mapquestapi.com/staticmap/v5/map?key=.............&center=" + COORDS + "&size=320,260&zoom=" + String(zoom) + "&size=@4x&locations=" + COORDS + "|marker-start");
  Serial.print("[HTTP] GET...");
  // start connection and send HTTP header
  int httpCode = http.GET();
  // httpCode will be negative on error
  if (httpCode > 0) {
    // HTTP header has been sent and Server response header has been handled
    Serial.printf(" code: %d\n", httpCode);
    // file found at server
    if (httpCode == 200) {
      int ln = http.getSize();
      Serial.println("\nPayload: " + String(ln));
      File file = SD.open("/MAP.JPG", FILE_WRITE);
      if (!file) {
        Serial.println("Failed to open file for writing");
      } else {
        Serial.println("writeToStream (file)");
        http.writeToStream(&file);
        file.close();
        Serial.println("drawJpgFile (file)");
        M5.Lcd.drawJpgFile(SD, "/MAP.JPG", 0, 0, 320, 240);
      }
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error %i: %s\n", httpCode, http.errorToString(httpCode).c_str());
  }
  http.end();
}

boolean checkConnection() {
  int count = 0;
  Serial.print("Waiting for Wi-Fi connection");
  while (count < 30) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected!");
      return (true);
    }
    delay(500);
    Serial.print(".");
    count++;
  }
  Serial.println("Timed out.");
  return false;
}

String getdms(double ang, bool isLat = true) {
  bool neg(false);
  if (ang < 0.0) {
    neg = true;
    ang = -ang;
  }
  int deg = (int)ang;
  double frac = ang - (double)deg;
  frac *= 60.0;
  int min = (int)frac;
  frac = frac - (double)min;
  double sec = nearbyint(frac * 600000.0);
  sec /= 10000.0;
  if (sec >= 60.0) {
    min++;
    sec -= 60.0;
  }
  String oss;
  if (neg) oss = "-";
  oss += String(deg) + "d " + String(min) + "' " + String(sec) + "\"";
  if (isLat) {
    if (neg) oss += "S";
    else oss += "N";
  } else {
    if (neg) oss += "W";
    else oss += "E";
  }
  return oss;
}

void setup() {
  lastMapDraw = 0;
  Serial.begin(115200);
  delay(1000);
  M5.begin();
  // Lcd display
  M5.Lcd.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Lcd.fillScreen(TFT_WHITE);
  M5.Lcd.setFreeFont(&FreeMono9pt7b);
  // the gpsSerial baud rate
  //gpsSerial.begin(9600);
  gpsSerial.begin(9600, SERIAL_8N1, 22, 21);
  // the Serial port of Arduino baud rate.
  Serial.println("\n\n+-----------------------+");
  Serial.println("+      GPS Tester       +");
  Serial.println("+-----------------------+");
  String wifi_ssid = "SecondTry";
  String wifi_password = "didier0barbas";
  Serial.print("WIFI-SSID: ");
  Serial.println(wifi_ssid);
  Serial.print("WIFI-PASSWD: ");
  Serial.println(wifi_password);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  bool x = false;
  while (!x) {
    Serial.write(' ');
    x = checkConnection();
  }
  Serial.println(WiFi.localIP());
}

void loadUpToDollar() {
  char c = gpsSerial.read();
  while (c != '$') {
    c = gpsSerial.read();
  }
}

int skipToNext(char *bf, char x, int pos) {
  int ix = pos;
  while (bf[ix] != x) {
    ix += 1;
  }
  return ix;
}

void loop() {
  if (gpsSerial.available()) {
    loadUpToDollar();
    while (gpsSerial.available()) {
      // reading data into char array
      char c = gpsSerial.read();
      buffer[count++] = c; // write data into array
      if (count == 128)break;
      if (c == 10)break;
      if (c == 13)break;
    }
    buffer[count] = 0;
    if (strncmp (buffer, "GPRMC", 5) == 0 || strncmp (buffer, "GNRMC", 5) == 0) {
      Serial.write("\n$");
      Serial.println(buffer);
      if (strncmp (buffer, "GPRMC,,", 7) == 0 || strncmp (buffer, "GNRMC,,", 7) == 0) goto TheEnd;
      // Invalid
      int ix = skipToNext(buffer, ',', 0) + 1;
      fixTime = String(buffer[ix++]) + String(buffer[ix++]) + ":" + String(buffer[ix++]) + String(buffer[ix++]) + ":" + String(buffer[ix++]) + String(buffer[ix++]) + " UTC";
      ix = skipToNext(buffer, ',', ix) + 1;
      char c = buffer[ix++];
      if (c != 'A') {
        hasFix = false;
      } else {
        double t1 = millis();
        //Serial.print("t1: "); Serial.println(t1);
        Serial.print("lastMapDraw: "); Serial.println(lastMapDraw);
        ix++;
        Serial.println("  [ok]");
        // Valid
        int yx = skipToNext(buffer, ',', ix);
        int i;
        String s = "";
        for (i = ix; i < yx; i++) s = s + String(buffer[i]);
        float Lat = s.toFloat();
        int DD = Lat / 100;
        float SS = Lat - DD * 100;
        float LatDec = DD + SS / 60;
        ix = skipToNext(buffer, ',', yx);
        s = String(buffer[ix]);
        if (s == "S") myLat *= -1;
        Serial.print("Latitude: ");
        myLatText = getdms(LatDec, true);
        Serial.println(myLatText);
        // Longitude
        ix += 3;
        yx = skipToNext(buffer, ',', ix);
        s = "";
        for (i = ix; i < yx; i++) s = s + String(buffer[i]);
        float Long = s.toFloat();
        DD = Long / 100;
        SS = Long - DD * 100;
        float LongDec = DD + SS / 60;
        ix = skipToNext(buffer, ',', yx);
        s = String(buffer[ix]);
        if (s == "W") myLong *= -1;
        Serial.println("Longitude: ");
        myLongText = getdms(LongDec, true);
        Serial.println(myLongText);
        ix = skipToNext(buffer, ',', yx);
        yx = ix + 1;
        ix = skipToNext(buffer, ',', yx);
        yx = ix + 1;
        ix = skipToNext(buffer, ',', yx);
        yx = ix + 1;
        ix = skipToNext(buffer, ',', yx);
        yx = ix + 1;
        fixDate = "20" + String(buffer[yx + 4]) + String(buffer[yx + 5]) + "/" + String(buffer[yx + 2]) + String(buffer[yx + 3]) + "/" + String(buffer[yx]) + String(buffer[yx + 1]);
        Serial.println("Fix taken at: " + fixTime + " on " + fixDate);
        hasFix = true;
        uint16_t elapsed = (t1 - lastMapDraw) / 1000;
        Serial.println("Time elapsed since last map draw: " + String(elapsed) + " s");
        if (t1 - lastMapDraw > 60000 || noFixSoFar) {
          COORDS = String(LatDec, 6) + "," + String(LongDec, 6);
          drawMap();
          lastMapDraw = millis();
          noFixSoFar = false;
        }
      }
    }
TheEnd:
    clearBufferArray(); // call clearBufferArray function to clear the storaged data from the array
    count = 0; // set counter of while loop to zero
  }
  buttons_test();
  M5.update(); // 好importantですね！
  // If not the buttons status is not updated lo.
}

void clearBufferArray() {
  // function to clear buffer array
  for (int i = 0; i < count; i++) {
    buffer[i] = NULL;
  } // clear all index of array with command NULL
}
