
#include <Wire.h>
#include <Adafruit_VEML7700.h>
#include <BH1750.h>

Adafruit_VEML7700 veml7700;
BH1750 bh1750(0x23);
const int BUTTON_PIN = 5;
const int BUTTON_ACTIVE = HIGH;
bool vemlReady = false;
bool bhReady = false;

bool lastButtonState = false;

unsigned long lastReadTime = 0;
const unsigned long READ_INTERVAL = 1000;


void scanI2C() {

  Serial.println();
  Serial.println(">>>>>>scan I2C address<<<<<<<");

  int deviceCount = 0;

  for (byte address = 1; address < 127; address++) {

    Wire.beginTransmission(address);

    byte error = Wire.endTransmission();

    if (error == 0) {

      Serial.print("Find I2C devices: 0x");

      if (address < 16) {
        Serial.print("0");
      }

      Serial.println(address, HEX);

      deviceCount++;
    }
  }

  Serial.print("number of device: ");
  Serial.println(deviceCount);

  Serial.println("----------------------------------");
}


void initVEML7700() {

  Serial.println();
  Serial.println("testing VEML7700...");

  if (veml7700.begin()) {

    vemlReady = true;

    veml7700.setGain(VEML7700_GAIN_1);
    veml7700.setIntegrationTime(VEML7700_IT_100MS);

    Serial.println("[SUCCESSFUL] VEML7700 contected!");
    Serial.println("I2C address: 0x10");

  } else {

    vemlReady = false;

    Serial.println("[FAILED] couldn't find VEML7700!");
    Serial.println("Please inspect SDA、SCL、VIN and GND。");
  }
}



void initBH1750() {

  Serial.println();
  Serial.println("testing BH1750...");

  bhReady = bh1750.begin(
    BH1750::CONTINUOUS_HIGH_RES_MODE,
    0x23,
    &Wire
  );

  if (bhReady) {

    Serial.println("[SUCCESSFUL] BH1750 connected!");
    Serial.println("I2C address: 0x23");

  } else {

    Serial.println("[FAILED] couldn't find BH1750!");
    Serial.println("Please inspect SDA、SCL、VCC、GND and ADDR。");
  }
}




void checkButton() {

  bool currentState =
    (digitalRead(BUTTON_PIN) == BUTTON_ACTIVE);

  if (currentState != lastButtonState) {

    delay(30);

    currentState =
      (digitalRead(BUTTON_PIN) == BUTTON_ACTIVE);

    if (currentState != lastButtonState) {

      if (currentState) {
        Serial.println("[BUTTON] It has been pressed.");
      } else {
        Serial.println("[BUTTON] It has been released.");
      }

      lastButtonState = currentState;
    }
  }
}



void readSensors() {

  unsigned long currentTime = millis();

  if (currentTime - lastReadTime < READ_INTERVAL) {
    return;
  }

  lastReadTime = currentTime;

  Serial.println();
  Serial.println("---------------Light Data-------------------");


  if (vemlReady) {

    float luxLeft = veml7700.readLux();

    Serial.print(" VEML7700: ");
    Serial.print(luxLeft);
    Serial.println(" Lux");

  } else {

    Serial.println(" VEML7700: not connected");
  }



  if (bhReady) {

    float luxRight = bh1750.readLightLevel();

    Serial.print(" BH1750: ");

    if (luxRight >= 0) {
      Serial.print(luxRight);
      Serial.println(" Lux");
    } else {
      Serial.println("Read failure!");
    }

  } else {

    Serial.println(" BH1750: not connected");
  }

  Serial.println("----------------------------------");
}


void setup() {

  Serial.begin(115200);

  unsigned long startTime = millis();

  while (!Serial && millis() - startTime < 5000) {
  }

  Serial.println();
  Serial.println("-------------------------------");
  Serial.println(" MKR WiFi 1010 check routine");
  Serial.println("-------------------------------");


  Wire.begin();
  Wire.setClock(100000);

  pinMode(BUTTON_PIN, INPUT);

  Serial.println();
  Serial.println("start scanning...");

  scanI2C();

  initVEML7700();
  initBH1750();

  lastButtonState =
    (digitalRead(BUTTON_PIN) == BUTTON_ACTIVE);
  Serial.println();
  Serial.println("-------- Check result --------");

  Serial.print("VEML7700: ");
  Serial.println(vemlReady ? "READY" : "FAILED");

  Serial.print("BH1750: ");
  Serial.println(bhReady ? "READY" : "FAILED");

  Serial.println("Button: please press the button");

  Serial.println("------------------------------");
  Serial.println();
  Serial.println("start checking...");
}



void loop() {

  checkButton();

  readSensors();

}
