#include <Wire.h>
#include <Adafruit_VEML7700.h>
#include <SoftwareWire.h>
#include <WiFiNINA.h>
#include <utility/wifi_drv.h>
#include <PubSubClient.h>

const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";
const char* MQTT_BROKER = "YOUR_MQTT_BROKER";
const int MQTT_PORT = YOUR_MQTT_PORT;
const char* MQTT_USERNAME = "YOUR_USERNAME";
const char* MQTT_PASSWORD = "YOUR_PASSWORD";

String lightId = "YOUR_ID";
String mqtt_topic = "ADDRESS..." + lightId;

const int num_leds = 72;
const int payload_size = num_leds * 3;
byte RGBpayload[payload_size];

WiFiClient wifiClient;
PubSubClient client(wifiClient);

Adafruit_VEML7700 veml1 = Adafruit_VEML7700();
SoftwareWire softWire(4, 3); // use the software to simulate the I2C address of the right sensor

const int BUTTON_PIN = 5;


bool lightOn = true;
enum LightStyle { STYLE_CONSTANT, STYLE_BREATHING, STYLE_BLINKING, STYLE_RANDOM };
LightStyle currentStyle = STYLE_CONSTANT;

const byte COLORS[][3] = {
  {255, 0, 0},
  {0, 255, 0},
  {0, 0, 255},
  {255, 0, 255},
  {0, 255, 255}
};
int colorIndex = 0;
const int totalColors = 5;

const float LIGHT_THRESHOLD = 20.0;
bool leftWasCovered = false;


//////
////// Click and long-press input detection for the newly added button sensor//////
unsigned long buttonPressStartTime = 0;
bool buttonIsPressed = false;
const unsigned long LONG_PRESS_TIME = 800;
bool longPressHandled = false;

void startWifi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi connected!");
  } else {
    Serial.println("Wi-Fi connection failed.");
  }
}

void reconnectMQTT() {
  while (!client.connected()) {
    if (WiFi.status() != WL_CONNECTED) startWifi();
    if (WiFi.status() != WL_CONNECTED) { delay(2000); continue; }

    Serial.print("Connecting to MQTT broker...");
    String clientId = "MKR1010-CASA0014-" + lightId;
    bool connected = (strlen(MQTT_USERNAME) > 0) ? 
                     client.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD) : 
                     client.connect(clientId.c_str());

    if (connected) {
      Serial.println(" connected!");
    } else {
      Serial.print(" failed, state = ");
      Serial.println(client.state());
      delay(3000);
    }
  }
}


float readRightSensorLux() {
  softWire.beginTransmission(0x10);
  softWire.write(0x04);
  if (softWire.endTransmission() != 0) {
    return 50.0;
  }
  softWire.requestFrom((uint8_t)0x10, (uint8_t)2);
  if (softWire.available() >= 2) {
    uint16_t lsb = softWire.read();
    uint16_t msb = softWire.read();
    uint16_t raw = (msb << 8) | lsb;
    return raw * 0.0576;
  }
  return 50.0;
}


void setLocalLEDColor(int r, int g, int b) {
  WiFiDrv::analogWrite(26, r);
  WiFiDrv::analogWrite(25, g);
  WiFiDrv::analogWrite(27, b);
}



void handleButtonAndSensors() {
  // Click or long-press the button to change the lighting style
  int btnState = digitalRead(BUTTON_PIN);
  if (btnState == HIGH && !buttonIsPressed) {
    buttonIsPressed = true;
    buttonPressStartTime = millis();
    longPressHandled = false;
  } else if (btnState == HIGH && buttonIsPressed) {
    if (!longPressHandled && (millis() - buttonPressStartTime >= LONG_PRESS_TIME)) {
      currentStyle = (LightStyle)((currentStyle + 1) % 4);
      Serial.print("---->>>> [LONG-PRESS] switch the lighting style to: ");
      Serial.println(currentStyle);
      longPressHandled = true;
    }
  } else if (btnState == LOW && buttonIsPressed) {
    unsigned long pressDuration = millis() - buttonPressStartTime;
    if (!longPressHandled && pressDuration < LONG_PRESS_TIME && pressDuration > 50) {
      lightOn = !lightOn;
      Serial.print("---->>>> [CLICK] light switch: ");
      Serial.println(lightOn ? "ON" : "OFF");
    }
    buttonIsPressed = false;
  }


  float luxLeft = veml1.readLux();
  bool leftCurrentlyCovered = (luxLeft < LIGHT_THRESHOLD);
  if (leftCurrentlyCovered && !leftWasCovered) {
    colorIndex = (colorIndex + 1) % totalColors;
    Serial.print("---- Only the left sensor is covered: ");
    Serial.println(colorIndex);
    delay(200); 
  }
  leftWasCovered = leftCurrentlyCovered;

  // Key change: The right sensor detects the intensity of the ambient light.
  float luxRight = readRightSensorLux();
  float brightness = constrain(luxRight / 150.0, 0.15, 1.0);

  byte baseR = COLORS[colorIndex][0];
  byte baseG = COLORS[colorIndex][1];
  byte baseB = COLORS[colorIndex][2];

  byte finalR = 0, finalG = 0, finalB = 0;

  if (lightOn) {
    switch (currentStyle) {
      case STYLE_CONSTANT:
        finalR = baseR; finalG = baseG; finalB = baseB;
        break;
      case STYLE_BREATHING: {
        float factor = (sin(millis() / 300.0) + 1.0) / 2.0;
        finalR = baseR * factor; finalG = baseG * factor; finalB = baseB * factor;
        break;
      }
      case STYLE_BLINKING:
        if ((millis() / 300) % 2 != 0) {
          finalR = 0; finalG = 0; finalB = 0;
        } else {
          finalR = baseR; finalG = baseG; finalB = baseB;
        }
        break;
      case STYLE_RANDOM:
        finalR = random(50, 256);
        finalG = random(50, 256);
        finalB = random(50, 256);
        break;
    }
    finalR *= brightness;
    finalG *= brightness;
    finalB *= brightness;
  }


  setLocalLEDColor(finalR, finalG, finalB);

  for (int i = 0; i < num_leds; i++) {
    if (lightOn && currentStyle == STYLE_RANDOM) {
      RGBpayload[i * 3 + 0] = random(50, 256) * brightness;
      RGBpayload[i * 3 + 1] = random(50, 256) * brightness;
      RGBpayload[i * 3 + 2] = random(50, 256) * brightness;
    } else {
      RGBpayload[i * 3 + 0] = finalR;
      RGBpayload[i * 3 + 1] = finalG;
      RGBpayload[i * 3 + 2] = finalB;
    }
  }

  if (client.connected()) {
    client.publish(mqtt_topic.c_str(), RGBpayload, payload_size);
  }
}

void setup() {
  Serial.begin(115200);
  unsigned long startTime = millis();
  while (!Serial && millis() - startTime < 5000) {}

  WiFiDrv::pinMode(25, OUTPUT);
  WiFiDrv::pinMode(26, OUTPUT);
  WiFiDrv::pinMode(27, OUTPUT);

  pinMode(BUTTON_PIN, INPUT);

  if (!veml1.begin()) {
    Serial.println("Couldn't find left VEML7700 sensor!");
  } else {
    veml1.setGain(VEML7700_GAIN_1);
    veml1.setIntegrationTime(VEML7700_IT_100MS);
  }

  softWire.begin();

  memset(RGBpayload, 0, sizeof(RGBpayload));
  startWifi();
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setBufferSize(256);
  reconnectMQTT();
}


void loop() {
  if (!client.connected()) reconnectMQTT();
  if (WiFi.status() != WL_CONNECTED) startWifi();
  client.loop();

  handleButtonAndSensors();

  delay(100); 
}