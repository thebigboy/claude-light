#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <ESP_I2S.h>
#include <Wire.h>

#include "chinese_font.h"
#include "confirm_prompt_mp3.h"

#ifndef RGB_BUILTIN
#define RGB_BUILTIN 48
#endif

namespace {

constexpr char DEVICE_NAME[] = "ClaudeLight";
constexpr char SERVICE_UUID[] = "7d6c1000-7a93-4b8b-9d55-4b31f058a501";
constexpr char STATE_CHARACTERISTIC_UUID[] = "7d6c1001-7a93-4b8b-9d55-4b31f058a501";
constexpr bool SHOW_RED_WHEN_DISCONNECTED = false;
constexpr int I2S_BCLK_PIN = 15;
constexpr int I2S_LRC_PIN = 16;
constexpr int I2S_DOUT_PIN = 7;
constexpr int OLED_SDA_PIN = 41;
constexpr int OLED_SCL_PIN = 42;
constexpr int TRAFFIC_RED_PIN = 4;
constexpr int TRAFFIC_YELLOW_PIN = 5;
constexpr int TRAFFIC_GREEN_PIN = 6;
constexpr int TRAFFIC_PWM_FREQUENCY = 5000;
constexpr int TRAFFIC_PWM_RESOLUTION = 8;
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr int OLED_WIDTH = 128;
constexpr int OLED_HEIGHT = 32;
constexpr int CONFIRMATION_PLAY_COUNT = 2;
constexpr unsigned long CONFIRMATION_PLAY_GAP_MS = 180;

enum class ClaudeState {
  Idle,
  Thinking,
  AwaitingConfirmation,
  Working,
  Error,
};

ClaudeState currentState = ClaudeState::Idle;
bool deviceConnected = false;
unsigned long connectionAnimationStart = 0;
volatile bool confirmationAudioRequested = false;
I2SClass i2s;
bool oledReady = false;
uint8_t oledBuffer[OLED_WIDTH * OLED_HEIGHT / 8] = {};

constexpr unsigned long CONNECTION_ANIMATION_STEP_MS = 140;
constexpr unsigned long CONNECTION_ANIMATION_STEPS = 9;

const uint8_t FONT[][5] = {
    {0x7E, 0x11, 0x11, 0x11, 0x7E},  // A
    {0x7F, 0x49, 0x49, 0x49, 0x36},  // B
    {0x3E, 0x41, 0x41, 0x41, 0x22},  // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C},  // D
    {0x7F, 0x49, 0x49, 0x49, 0x41},  // E
    {0x7F, 0x09, 0x09, 0x09, 0x01},  // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A},  // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F},  // H
    {0x00, 0x41, 0x7F, 0x41, 0x00},  // I
    {0x20, 0x40, 0x41, 0x3F, 0x01},  // J
    {0x7F, 0x08, 0x14, 0x22, 0x41},  // K
    {0x7F, 0x40, 0x40, 0x40, 0x40},  // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F},  // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F},  // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E},  // O
    {0x7F, 0x09, 0x09, 0x09, 0x06},  // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E},  // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46},  // R
    {0x46, 0x49, 0x49, 0x49, 0x31},  // S
    {0x01, 0x01, 0x7F, 0x01, 0x01},  // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F},  // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F},  // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F},  // W
    {0x63, 0x14, 0x08, 0x14, 0x63},  // X
    {0x07, 0x08, 0x70, 0x08, 0x07},  // Y
    {0x61, 0x51, 0x49, 0x45, 0x43},  // Z
};

void setRgb(uint8_t red, uint8_t green, uint8_t blue) {
  rgbLedWrite(RGB_BUILTIN, red, green, blue);

  ledcWrite(TRAFFIC_RED_PIN, 0);
  ledcWrite(TRAFFIC_YELLOW_PIN, 0);
  ledcWrite(TRAFFIC_GREEN_PIN, 0);
  if (red > 0 && green > 0) {
    ledcWrite(TRAFFIC_YELLOW_PIN, max(red, green));
  } else if (red > 0) {
    ledcWrite(TRAFFIC_RED_PIN, red);
  } else if (green > 0) {
    ledcWrite(TRAFFIC_GREEN_PIN, green);
  }
}

bool initializeTrafficLight() {
  const bool redReady = ledcAttach(TRAFFIC_RED_PIN, TRAFFIC_PWM_FREQUENCY, TRAFFIC_PWM_RESOLUTION);
  const bool yellowReady = ledcAttach(TRAFFIC_YELLOW_PIN, TRAFFIC_PWM_FREQUENCY, TRAFFIC_PWM_RESOLUTION);
  const bool greenReady = ledcAttach(TRAFFIC_GREEN_PIN, TRAFFIC_PWM_FREQUENCY, TRAFFIC_PWM_RESOLUTION);
  setRgb(0, 0, 0);
  return redReady && yellowReady && greenReady;
}

void runTrafficLightSelfTest() {
  Serial.println("Traffic light self-test: RED only");
  ledcWrite(TRAFFIC_RED_PIN, 24);
  ledcWrite(TRAFFIC_YELLOW_PIN, 0);
  ledcWrite(TRAFFIC_GREEN_PIN, 0);
  delay(700);

  Serial.println("Traffic light self-test: YELLOW only");
  ledcWrite(TRAFFIC_RED_PIN, 0);
  ledcWrite(TRAFFIC_YELLOW_PIN, 24);
  ledcWrite(TRAFFIC_GREEN_PIN, 0);
  delay(700);

  Serial.println("Traffic light self-test: GREEN only");
  ledcWrite(TRAFFIC_RED_PIN, 0);
  ledcWrite(TRAFFIC_YELLOW_PIN, 0);
  ledcWrite(TRAFFIC_GREEN_PIN, 24);
  delay(700);

  ledcWrite(TRAFFIC_RED_PIN, 0);
  ledcWrite(TRAFFIC_YELLOW_PIN, 0);
  ledcWrite(TRAFFIC_GREEN_PIN, 0);
}

void oledCommand(uint8_t command) {
  Wire.beginTransmission(OLED_ADDRESS);
  Wire.write(0x00);
  Wire.write(command);
  Wire.endTransmission();
}

void oledPixel(int x, int y, bool on = true) {
  if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) {
    return;
  }
  const int index = x + (y / 8) * OLED_WIDTH;
  const uint8_t mask = 1 << (y % 8);
  if (on) {
    oledBuffer[index] |= mask;
  } else {
    oledBuffer[index] &= ~mask;
  }
}

void oledLine(int x0, int y0, int x1, int y1) {
  const int dx = abs(x1 - x0);
  const int sx = x0 < x1 ? 1 : -1;
  const int dy = -abs(y1 - y0);
  const int sy = y0 < y1 ? 1 : -1;
  int error = dx + dy;

  while (true) {
    oledPixel(x0, y0);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    const int twiceError = 2 * error;
    if (twiceError >= dy) {
      error += dy;
      x0 += sx;
    }
    if (twiceError <= dx) {
      error += dx;
      y0 += sy;
    }
  }
}

void oledCircle(int centerX, int centerY, int radius) {
  int x = radius;
  int y = 0;
  int error = 0;
  while (x >= y) {
    oledPixel(centerX + x, centerY + y);
    oledPixel(centerX + y, centerY + x);
    oledPixel(centerX - y, centerY + x);
    oledPixel(centerX - x, centerY + y);
    oledPixel(centerX - x, centerY - y);
    oledPixel(centerX - y, centerY - x);
    oledPixel(centerX + y, centerY - x);
    oledPixel(centerX + x, centerY - y);
    ++y;
    if (error <= 0) {
      error += 2 * y + 1;
    }
    if (error > 0) {
      --x;
      error -= 2 * x + 1;
    }
  }
}

void oledText(const char *text, int x, int y, int scale = 1) {
  while (*text) {
    const char character = *text++;
    if (character == ' ') {
      x += 6 * scale;
      continue;
    }
    if (character < 'A' || character > 'Z') {
      continue;
    }
    const uint8_t *glyph = FONT[character - 'A'];
    for (int column = 0; column < 5; ++column) {
      for (int row = 0; row < 7; ++row) {
        if ((glyph[column] >> row) & 1) {
          for (int sx = 0; sx < scale; ++sx) {
            for (int sy = 0; sy < scale; ++sy) {
              oledPixel(x + column * scale + sx, y + row * scale + sy);
            }
          }
        }
      }
    }
    x += 6 * scale;
  }
}

void oledChineseGlyph(uint32_t codepoint, int x, int y) {
  const ChineseGlyph *glyph = nullptr;
  for (size_t index = 0; index < CHINESE_GLYPH_COUNT; ++index) {
    if (CHINESE_GLYPHS[index].codepoint == codepoint) {
      glyph = &CHINESE_GLYPHS[index];
      break;
    }
  }
  if (glyph == nullptr) {
    return;
  }

  for (int row = 0; row < 16; ++row) {
    const uint16_t pixels =
        (static_cast<uint16_t>(glyph->rows[row * 2]) << 8) |
        glyph->rows[row * 2 + 1];
    for (int column = 0; column < 16; ++column) {
      if (pixels & (0x8000 >> column)) {
        oledPixel(x + column, y + row);
      }
    }
  }
}

void oledChineseText(const uint32_t *text, size_t length, int x, int y) {
  for (size_t index = 0; index < length; ++index) {
    oledChineseGlyph(text[index], x + index * 18, y);
  }
}

void oledFlush() {
  if (!oledReady) {
    return;
  }
  oledCommand(0x21);
  oledCommand(0);
  oledCommand(OLED_WIDTH - 1);
  oledCommand(0x22);
  oledCommand(0);
  oledCommand(3);
  for (int offset = 0; offset < sizeof(oledBuffer); offset += 16) {
    Wire.beginTransmission(OLED_ADDRESS);
    Wire.write(0x40);
    Wire.write(oledBuffer + offset, 16);
    Wire.endTransmission();
  }
}

void drawDisplay() {
  if (!oledReady) {
    return;
  }

  memset(oledBuffer, 0, sizeof(oledBuffer));
  const unsigned long now = millis();

  if (!deviceConnected) {
    const uint32_t text[] = {0x672A, 0x8FDE, 0x63A5};  // 未连接
    oledCircle(16, 16, 9);
    oledLine(9, 23, 23, 9);
    oledChineseText(text, 3, 40, 8);
    oledFlush();
    return;
  }

  switch (currentState) {
    case ClaudeState::Idle: {
      const uint32_t text[] = {0x7A7A, 0x95F2};  // 空闲
      oledCircle(16, 16, 9);
      oledChineseText(text, 2, 48, 8);
      break;
    }
    case ClaudeState::Thinking: {
      const uint32_t text[] = {0x601D, 0x8003, 0x4E2D};  // 思考中
      const int frame = (now / 180) % 4;
      oledCircle(16, 16, 9);
      if (frame == 0) oledLine(16, 4, 16, 10);
      if (frame == 1) oledLine(28, 16, 22, 16);
      if (frame == 2) oledLine(16, 28, 16, 22);
      if (frame == 3) oledLine(4, 16, 10, 16);
      oledChineseText(text, 3, 40, 8);
      break;
    }
    case ClaudeState::AwaitingConfirmation: {
      const uint32_t text[] = {0x8BF7, 0x786E, 0x8BA4};  // 请确认
      oledLine(16, 5, 4, 27);
      oledLine(4, 27, 28, 27);
      oledLine(28, 27, 16, 5);
      oledLine(16, 11, 16, 20);
      oledPixel(16, 24);
      if ((now / 300) % 2 == 0) {
        oledChineseText(text, 3, 40, 8);
      }
      break;
    }
    case ClaudeState::Working: {
      const uint32_t text[] = {0x5DE5, 0x4F5C, 0x4E2D};  // 工作中
      oledCircle(16, 16, 9);
      oledCircle(16, 16, 3);
      oledLine(16, 3, 16, 7);
      oledLine(16, 25, 16, 29);
      oledLine(3, 16, 7, 16);
      oledLine(25, 16, 29, 16);
      oledChineseText(text, 3, 40, 8);
      break;
    }
    case ClaudeState::Error: {
      const uint32_t text[] = {0x9519, 0x8BEF};  // 错误
      oledLine(7, 7, 25, 25);
      oledLine(25, 7, 7, 25);
      oledChineseText(text, 2, 48, 8);
      break;
    }
  }
  oledFlush();
}

bool initializeDisplay() {
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  Wire.beginTransmission(OLED_ADDRESS);
  if (Wire.endTransmission() != 0) {
    return false;
  }

  const uint8_t commands[] = {
      0xAE, 0xD5, 0x80, 0xA8, 0x1F, 0xD3, 0x00, 0x40,
      0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x02,
      0x81, 0x7F, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6,
      0xAF,
  };
  for (uint8_t command : commands) {
    oledCommand(command);
  }
  oledReady = true;
  drawDisplay();
  return true;
}

bool updateState(const String &value) {
  const ClaudeState previousState = currentState;

  if (value == "idle") {
    currentState = ClaudeState::Idle;
  } else if (value == "thinking") {
    currentState = ClaudeState::Thinking;
  } else if (value == "awaiting_confirmation") {
    currentState = ClaudeState::AwaitingConfirmation;
  } else if (value == "working") {
    currentState = ClaudeState::Working;
  } else if (value == "error") {
    currentState = ClaudeState::Error;
  } else {
    return false;
  }

  if (currentState == ClaudeState::AwaitingConfirmation &&
      previousState != ClaudeState::AwaitingConfirmation) {
    confirmationAudioRequested = true;
  }

  Serial.printf("State: %s\n", value.c_str());
  return true;
}

void audioTask(void *) {
  while (true) {
    if (!confirmationAudioRequested) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

    confirmationAudioRequested = false;
    Serial.println("Playing confirmation prompt");

    for (int count = 0; count < CONFIRMATION_PLAY_COUNT; ++count) {
      if (!i2s.playMP3(confirm_prompt_mp3, confirm_prompt_mp3_len)) {
        Serial.println("Failed to play confirmation prompt");
        break;
      }

      if (count + 1 < CONFIRMATION_PLAY_COUNT) {
        vTaskDelay(pdMS_TO_TICKS(CONFIRMATION_PLAY_GAP_MS));
      }
    }
  }
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *) override {
    deviceConnected = true;
    connectionAnimationStart = millis();
    Serial.println("BLE client connected");
  }

  void onDisconnect(BLEServer *server) override {
    deviceConnected = false;
    Serial.println("BLE client disconnected; advertising restarted");
    server->getAdvertising()->start();
  }
};

class StateCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    String value(characteristic->getValue().c_str());
    value.trim();

    if (!updateState(value)) {
      Serial.printf("Unknown state: %s\n", value.c_str());
    }
  }
};

void updateLight() {
  const unsigned long now = millis();

  if (!deviceConnected) {
    if (SHOW_RED_WHEN_DISCONNECTED) {
      setRgb(24, 0, 0);
    } else {
      setRgb(0, 0, 0);
    }
    return;
  }

  const unsigned long animationElapsed = now - connectionAnimationStart;
  const unsigned long animationStep = animationElapsed / CONNECTION_ANIMATION_STEP_MS;
  if (animationStep < CONNECTION_ANIMATION_STEPS) {
    switch (animationStep % 3) {
      case 0:
        setRgb(24, 0, 0);
        break;
      case 1:
        setRgb(24, 16, 0);
        break;
      default:
        setRgb(0, 24, 0);
        break;
    }
    return;
  }

  switch (currentState) {
    case ClaudeState::Idle:
      setRgb(24, 0, 0);
      break;

    case ClaudeState::Thinking: {
      const uint8_t brightness = 2 + (now % 1600 < 800 ? now % 800 : 800 - now % 800) * 18 / 800;
      setRgb(brightness, brightness * 3 / 4, 0);
      break;
    }

    case ClaudeState::AwaitingConfirmation:
      if ((now / 180) % 2 == 0) {
        setRgb(24, 16, 0);
      } else {
        setRgb(0, 0, 0);
      }
      break;

    case ClaudeState::Working: {
      const uint8_t brightness = 2 + (now % 1600 < 800 ? now % 800 : 800 - now % 800) * 18 / 800;
      setRgb(0, brightness, 0);
      break;
    }

    case ClaudeState::Error:
      if ((now / 250) % 2 == 0) {
        setRgb(24, 2, 0);
      } else {
        setRgb(0, 0, 0);
      }
      break;
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);

  delay(2000);

  Serial.println();

  Serial.println("Booting ClaudeLight...");

  BLEDevice::init(DEVICE_NAME);
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService *service = server->createService(SERVICE_UUID);
  BLECharacteristic *stateCharacteristic = service->createCharacteristic(
      STATE_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_WRITE_NR);

  stateCharacteristic->setValue("idle");
  stateCharacteristic->setCallbacks(new StateCallbacks());
  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->start();

  Serial.println("ClaudeLight BLE ready");
  Serial.printf("Device: %s\n", DEVICE_NAME);
  Serial.printf("Service: %s\n", SERVICE_UUID);
  Serial.printf("State characteristic: %s\n", STATE_CHARACTERISTIC_UUID);

  if (initializeTrafficLight()) {
    Serial.println("External traffic light ready");
    runTrafficLightSelfTest();
  } else {
    Serial.println("Failed to initialize external traffic light");
  }

  if (initializeDisplay()) {
    Serial.println("OLED display ready");
  } else {
    Serial.println("OLED display not found at address 0x3C");
  }

  i2s.setPins(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DOUT_PIN);
  if (!i2s.begin(
          I2S_MODE_STD,
          24000,
          I2S_DATA_BIT_WIDTH_16BIT,
          I2S_SLOT_MODE_MONO,
          I2S_STD_SLOT_LEFT)) {
    Serial.println("Failed to initialize I2S audio");
  } else {
    xTaskCreatePinnedToCore(audioTask, "confirmation-audio", 8192, nullptr, 1, nullptr, 1);
    Serial.println("I2S audio ready");
  }
}

void loop() {
  updateLight();
  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate >= 250) {
    lastDisplayUpdate = millis();
    drawDisplay();
  }
  delay(16);
}
