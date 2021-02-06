#include "config.h"

#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

#include "AdafruitIO_WiFi.h"
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

#include "Adafruit_NeoPixel.h"
#define PIXEL_PIN     D4
#define PIXEL_COUNT   8
#define PIXEL_TYPE    NEO_GRB + NEO_KHZ800
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);
AdafruitIO_Feed *IOcolor = io.feed("friendship.color");

#define BUTTON_PIN D2
#define TOUCH_PIN D1

#define TOUCH_DELAY 3500
#define SLEEP_DELAY 30000
unsigned long last_touch_update = 0;
unsigned long last_sleep_update = 0;
bool touched = false;
bool sleeping = false;

// https://www.color-hex.com/
// Define colors
uint32_t colors[] = {0xff0000, 0xFF7F00, 0xFFFF00, 0x00FF00, 0x0000FF, 0xb6fff4, 0x2E2B5F, 0x8B00FF, 0x833827, 0x010101};
int num_colors = sizeof(colors) / sizeof(colors[0]);

int current_color = -1;
bool touch_event = false;

void ICACHE_RAM_ATTR ISRhandleTouch() {
  touch_event = true;
  //Serial.println("Touch event received");
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void wifiManagerInit() {
  WiFiManager wifiManager;
  //reset settings - for testing
  //wifiManager.resetSettings();

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  if (!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  Serial.println(WiFi.localIP());

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
}

void setup() {
  Serial.begin(115200);
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(TOUCH_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(TOUCH_PIN), ISRhandleTouch, FALLING);

  Serial.print("Connecting to Adafruit IO");
  io.connect();

  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  
  Serial.println();
  Serial.println(io.statusText());

  IOcolor->onMessage(handleMessage);
  IOcolor->get();

  pixels.begin();
  pixels.show();
  pixels.setBrightness(250);

  last_sleep_update = millis();
  
  //wifiManagerInit();
  OTAinit();
  
}

void loop() {

  io.run();
  ArduinoOTA.handle();
    
  if (Serial.available() > 0) {
    String str;
    str = Serial.readStringUntil('\n');
    if (str == "t") {
      Serial.print("Touched:");
      handleTouch();
    }
  }
  if (touch_event == true) {
    handleTouch();
    touch_event = false;
  }
  
  int button_state = digitalRead(BUTTON_PIN);
  if (button_state == 0) {
    sendColor(colors[9]);
    delay(10);
  }
  
  if (touched && millis() > (last_touch_update + TOUCH_DELAY)) {
    sendColor(colors[current_color]);
    Serial.print("Sending color:");
    Serial.println(colors[current_color], HEX);
    touched = false;
    current_color = -1;
  }

  if (!sleeping && millis() > (last_sleep_update + SLEEP_DELAY)) {
    Serial.println("Sleeping");
    setLocalColor(0x000000);
    last_sleep_update = millis();
    sleeping = true;
    current_color = -1;
  }

}

void handleMessage(AdafruitIO_Data *data) {

  uint32_t new_color = data->toUnsignedInt();
  Serial.print("Received color: ");
  Serial.println(new_color, HEX);

  last_sleep_update = millis();
  sleeping = false;

  setLocalColor(new_color);
}

void sendColor(uint32_t color) {
  IOcolor->save(color);
}

void handleTouch() {
  touched = true;
  sleeping = false;
  last_touch_update = last_sleep_update = millis();

  if (++current_color == num_colors) {
    current_color = 0;
  }

  Serial.print("[");
  Serial.print(current_color);
  Serial.print("]:");
  setLocalColor(colors[current_color]);
  Serial.println(colors[current_color], HEX);

}

void setLocalColor(uint32_t color) {
  if (color == 0x010101) { // Party mode
    partyMode();
  } else {
    for (int i = 0; i < PIXEL_COUNT; ++i) {
      pixels.setPixelColor(i, color);
      pixels.show();
      delay(30);
    }
  }
}

void partyMode() {
  for (int r = 0; r < 7; ++r) {
    for (int c = 0; c < num_colors-1; ++c) {
      for (int i = 0; i < PIXEL_COUNT; ++i) {
        pixels.setPixelColor(i, colors[c]);
      }
      pixels.show();
      last_touch_update = last_sleep_update = millis();

      delay(random(250));
    }
  }
  // Partying this hard makes one feel wearisome.
  setLocalColor(0x000000);
}

void OTAinit() {
  
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  char ota_name[20];
  sprintf(ota_name, "Friendship-%06x", ESP.getChipId());
  ArduinoOTA.setHostname(ota_name);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}
