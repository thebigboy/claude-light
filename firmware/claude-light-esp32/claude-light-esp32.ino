#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#ifndef RGB_BUILTIN
#define RGB_BUILTIN 48
#endif

namespace {

constexpr char DEVICE_NAME[] = "ClaudeLight";
constexpr char SERVICE_UUID[] = "7d6c1000-7a93-4b8b-9d55-4b31f058a501";
constexpr char STATE_CHARACTERISTIC_UUID[] = "7d6c1001-7a93-4b8b-9d55-4b31f058a501";

enum class ClaudeState {
  Idle,
  Thinking,
  AwaitingConfirmation,
  Working,
  Error,
};

ClaudeState currentState = ClaudeState::Idle;
bool deviceConnected = false;

void setRgb(uint8_t red, uint8_t green, uint8_t blue) {
  rgbLedWrite(RGB_BUILTIN, red, green, blue);
}

bool updateState(const String &value) {
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

  Serial.printf("State: %s\n", value.c_str());
  return true;
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *) override {
    deviceConnected = true;
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

  switch (currentState) {
    case ClaudeState::Idle:
      setRgb(255, 0, 0);
      break;

    case ClaudeState::Thinking: {
      const uint8_t brightness = 25 + (now % 1600 < 800 ? now % 800 : 800 - now % 800) * 180 / 800;
      setRgb(brightness, brightness * 3 / 4, 0);
      break;
    }

    case ClaudeState::AwaitingConfirmation:
      if ((now / 180) % 2 == 0) {
        setRgb(255, 170, 0);
      } else {
        setRgb(0, 0, 0);
      }
      break;

    case ClaudeState::Working: {
      const uint8_t brightness = 25 + (now % 1600 < 800 ? now % 800 : 800 - now % 800) * 180 / 800;
      setRgb(0, brightness, 0);
      break;
    }

    case ClaudeState::Error:
      if ((now / 250) % 2 == 0) {
        setRgb(255, 20, 0);
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
}

void loop() {
  updateLight();
  delay(16);
}
