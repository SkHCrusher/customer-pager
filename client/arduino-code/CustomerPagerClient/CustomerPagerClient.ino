#include <EasyButton.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFiManager.h>
#include <Preferences.h>

// Generell
bool pagerActive = false;
int pagerNumber;
unsigned long currentTime = 0; 

// Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Alarm-Icon
const int alarmIconSwitchTime = 400;
int currentAlarmIconFrame = 0; 
unsigned long lastAlarmIconTime = 0; 

// Bitmaps converted by http://javl.github.io/image2cpp/
// 'AlarmOn_26x26'
const unsigned char epd_bitmap_AlarmOn_26x26 [] PROGMEM = {
	0x1c, 0x00, 0x0e, 0x00, 0x38, 0x00, 0x07, 0x00, 0x60, 0x00, 0x01, 0x80, 0xc6, 0x00, 0x18, 0xc0, 
	0xcc, 0x00, 0x0c, 0xc0, 0x98, 0x00, 0x06, 0x40, 0x10, 0xff, 0xc2, 0x00, 0x01, 0x00, 0x20, 0x00, 
	0x02, 0x00, 0x10, 0x00, 0x02, 0x00, 0x10, 0x00, 0x3f, 0xff, 0xff, 0x00, 0x40, 0x00, 0x00, 0x80, 
	0x80, 0x00, 0x00, 0x40, 0x80, 0x00, 0x00, 0x40, 0xb9, 0xc4, 0xed, 0x40, 0xa9, 0x0e, 0x95, 0x40, 
	0xb9, 0xca, 0x96, 0x40, 0xa9, 0x1e, 0xb2, 0x40, 0xad, 0xd2, 0xe2, 0x40, 0x80, 0x00, 0x00, 0x40, 
	0x80, 0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x80, 0x3f, 0xff, 0xff, 0x00, 0x09, 0x00, 0x12, 0x00, 
	0x09, 0x00, 0x12, 0x00, 0x06, 0x00, 0x0c, 0x00
};
// 'AlarmOff_26x26'
const unsigned char epd_bitmap_AlarmOff_26x26 [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc0, 0x00, 0x01, 0x00, 0x20, 0x00, 
	0x02, 0x00, 0x10, 0x00, 0x02, 0x00, 0x10, 0x00, 0x3f, 0xff, 0xff, 0x00, 0x40, 0x00, 0x00, 0x80, 
	0x80, 0x00, 0x00, 0x40, 0x80, 0x00, 0x00, 0x40, 0xb9, 0xc4, 0xed, 0x40, 0xa9, 0x0e, 0x95, 0x40, 
	0xb9, 0xca, 0x96, 0x40, 0xa9, 0x1e, 0xb2, 0x40, 0xad, 0xd2, 0xe2, 0x40, 0x80, 0x00, 0x00, 0x40, 
	0x80, 0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x80, 0x3f, 0xff, 0xff, 0x00, 0x09, 0x00, 0x12, 0x00, 
	0x09, 0x00, 0x12, 0x00, 0x06, 0x00, 0x0c, 0x00
};

// Array of all bitmaps for convenience. (Total bytes used to store images in PROGMEM = 256)
const int epd_bitmap_allArray_LEN = 2;
const unsigned char* epd_bitmap_allArray[2] = {
	epd_bitmap_AlarmOff_26x26,
	epd_bitmap_AlarmOn_26x26
};

// Button - Blau
const int buttonPin = 18; 
int buttonWifiManagerPresses = 3;
int buttonWifiManagerTimeout = 1000;
const unsigned long buttonPressTime = 1000; 
EasyButton button(buttonPin, 35, true, false);

// LED - Orange
const int ledPin = 5;
const int ledSwitchTime = 200;
bool currentLedState = false; 
unsigned long lastLedTime = 0; 

// Vibration - braun
const int vibrationPin = 17;
const int vibrationDelay = 800;
bool currentVibrationState = false; 
unsigned long lastVibrationTime = 0; 

// Wifi
WiFiManager wm;
int wifiConfigurationPortalTimeout = 60;
char wifiName[32] = "CustomerPagerTest";
char wifiPassword[32] = "password";
char tempPagerNumber[3];
WiFiManagerParameter custom_pager_number("pager_number", "Pager Number", "", 3);

// Webserver
WebServer server(8080);

// Config
Preferences preferences;

void setup() {
  Serial.begin(115200);

  // Config
  loadConfig();

  // Display - Gelb - Grün
  
  Wire.begin(23, 19);
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  displayWifiConnection();
  
  // LED
  pinMode(ledPin, OUTPUT);
  
  // Vibration
  pinMode(vibrationPin, OUTPUT);
  
  // Button
  button.begin();
  button.onPressedFor(buttonPressTime, onPressedForDuration);
  button.onSequence(buttonWifiManagerPresses, buttonWifiManagerTimeout, onActivateWifiManager);

  // Wifi
  wifiSetup();
}

void loop() {
  currentTime = millis();

  button.read();
  alarmLoop();
  displayLoop();
  server.handleClient();
}

void loadConfig() {
  preferences.begin("pager-data", true);
  pagerNumber = preferences.getUInt("pagernumber", 0);
  preferences.end();
}

void apiAlarmStart() {
  activateAlarm();
  server.send(200, "text/plain", "Alarm accepted");
}

void apiAlarmStop() {
  deactivateAlarm();
  server.send(200, "text/plain", "Alarm accepted");
}

void wifiSetup() {
  // Add Customparameter and set default value
  wm.addParameter(&custom_pager_number);
  char tmpChar[3];
  itoa(pagerNumber, tmpChar, 10);
  custom_pager_number.setValue(tmpChar, 3);

  // Custom menu
  // Added param for custom configuration
  // Added exit to start loop after configuration
  std::vector<const char *> menu = {"wifi","info","param","close","sep","update","exit"};
  wm.setMenu(menu);

  // Setup Callback
  wm.setSaveParamsCallback(saveWifiParamsCallback);

  // Custom Routes
  //wm.setWebServerCallback(bindServerCallback);

  // Init Wifi
  bool res;
  res = wm.autoConnect(wifiName, wifiPassword);
  if(!res) {
    ESP.restart();
  }

  // Start new Webserver
  Serial.println("Custom Webserver");
  //wm.startWebPortal();
  
  server.on("/alarm-start", apiAlarmStart);
  server.on("/alarm-stop", apiAlarmStop);

  server.begin();
}

void bindServerCallback(){
  Serial.println("Routen eingerichtet");
  //wm.server->on("/alarm-start", apiAlarmStart);
  //wm.server->on("/alarm-stop", apiAlarmStop);
}

void saveWifiParamsCallback () {
  int newPagerNumber = atoi(custom_pager_number.getValue());
  pagerNumber = newPagerNumber;

  preferences.begin("pager-data", false);
  preferences.putUInt("pagernumber", pagerNumber);
  preferences.end();

  wm.stopConfigPortal();
}

void onPressedForDuration() {
  if (pagerActive) {
    deactivateAlarm();
    return;
  }

  activateAlarm();
}

void onActivateWifiManager() {
  displayWifiConnection();
  wm.setConfigPortalTimeout(wifiConfigurationPortalTimeout);
  wm.startConfigPortal(wifiName, wifiPassword);
}

void activateAlarm() {
  pagerActive = true;
  currentVibrationState = true;
  lastVibrationTime = currentTime;
}

void deactivateAlarm() {
  pagerActive = false;
  currentVibrationState = false;
}

void displayWifiConnection() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("Configure Wifi!");
  display.setCursor(0, 8);
  display.println("Networkname:");
  display.setCursor(0, 16);
  display.println(wifiName);

  display.display();
}

void displayLoop() {
  // Prepare
  display.clearDisplay();
  display.setTextSize(4);
  display.setTextColor(SSD1306_WHITE);

  // Pager number
  display.setCursor(0, 0);
  display.println(pagerNumber);

  // Alarm Icon
  if (pagerActive) {
    if ((lastAlarmIconTime + alarmIconSwitchTime) < currentTime) {
      currentAlarmIconFrame = (currentAlarmIconFrame == 0) ? 1 : 0;
      lastAlarmIconTime = currentTime;
    }
    
    display.drawBitmap(102, 0, epd_bitmap_allArray[currentAlarmIconFrame], 26, 26, SSD1306_WHITE);
  }

  // Update display
  display.display();
}

void alarmLoop() {
  if (pagerActive) {
    // Vibration
    if ((lastVibrationTime + vibrationDelay) < currentTime) {
      currentVibrationState = !currentVibrationState;
      lastVibrationTime = currentTime;
    }

    if (currentVibrationState) {
      digitalWrite(vibrationPin, HIGH);
    } else {
      digitalWrite(vibrationPin, LOW);
    }

    // LED
    if ((lastLedTime + ledSwitchTime) < currentTime) {
      currentLedState = !currentLedState;
      lastLedTime = currentTime;
    }

    if (currentLedState) {
      digitalWrite(ledPin, HIGH);
    } else {
      digitalWrite(ledPin, LOW);
    }
  } else {
    // Vibration Off
    digitalWrite(vibrationPin, LOW);

    // LED Off
    digitalWrite(ledPin, LOW);
  }
}