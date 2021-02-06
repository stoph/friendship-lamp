#include "config.h"

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

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(TOUCH_PIN, INPUT_PULLUP);
  
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  IOcolor->onMessage(handleMessage);

  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println(io.statusText());
  IOcolor->get();

  pixels.begin();
  pixels.show();
  pixels.setBrightness(250);

  last_sleep_update = millis();

}

void loop() {

  if (Serial.available() > 0) {
    String str;
    str = Serial.readStringUntil('\n');
    if (str == "t") {
      Serial.print("Touched:");
      handleTouch();
    }
  }

  int touch_state = digitalRead(TOUCH_PIN);
  if (touch_state == 0) {
    handleTouch();
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

  io.run();
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
}
