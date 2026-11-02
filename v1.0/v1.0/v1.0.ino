#include <Wire.h>
#include <Adafruit_VEML7700.h>
#include <WiFiNINA.h>
#include <utility/wifi_drv.h>
#include <PubSubClient.h>

const char* WIFI_SSID = "CE-Hub-Student";
const char* WIFI_PASSWORD = "casa-ce-gagarin-public-service";
const char* MQTT_BROKER = "mqtt.cetools.org";
const int MQTT_PORT = 1884;
const char* MQTT_USERNAME = "student";
const char* MQTT_PASSWORD = "ce2021-mqtt-forget-whale";
String lightId = "7";
String mqtt_topic = "student/CASA0014/luminaire/" + lightId;

const int num_leds = 72;
const int payload_size = num_leds * 3;
byte RGBpayload[payload_size];

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// Using the left light sensor with hardware I2C
Adafruit_VEML7700 veml1 = Adafruit_VEML7700(); 

// simulate the right-side light sensor of the I2C address through software
const int SOFT_SDA = 4;
const int SOFT_SCL = 3;

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

// use two sensors to simulate the effect of finger sliding
unsigned long leftCoverTime = 0;
bool leftCovered = false;
const unsigned long SWIPE_TIMEOUT = 800;


void startWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

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
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi connection failed.");
  }
}


void reconnectMQTT() {
  while (!client.connected()) {
    if (WiFi.status() != WL_CONNECTED) {
      startWifi();
    }

    if (WiFi.status() != WL_CONNECTED) {
      delay(2000);
      continue;
    }

    Serial.print("Connecting to MQTT broker...");
    String clientId = "MKR1010-CASA0014-" + lightId;
    bool connected;

    if (strlen(MQTT_USERNAME) > 0) {
      connected = client.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD);
    } else {
      connected = client.connect(clientId.c_str());
    }

    if (connected) {
      Serial.println(" connected!");
      Serial.print("MQTT topic: ");
      Serial.println(mqtt_topic);
    } else {
      Serial.print(" failed, state = ");
      Serial.println(client.state());
      delay(3000);
    }
  }
}


void setLocalLEDColor(int r, int g, int b) {
  WiFiDrv::analogWrite(26, r);
  WiFiDrv::analogWrite(25, g);
  WiFiDrv::analogWrite(27, b);
}


void updateAndPublishLights() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  byte baseR = COLORS[colorIndex][0];
  byte baseG = COLORS[colorIndex][1];
  byte baseB = COLORS[colorIndex][2];

  byte currentR = baseR;
  byte currentG = baseG;
  byte currentB = baseB;

  switch (currentStyle) {
    case STYLE_CONSTANT:
      break;
    case STYLE_BREATHING: {
      float factor = (sin(millis() / 300.0) + 1.0) / 2.0;
      currentR = baseR * factor;
      currentG = baseG * factor;
      currentB = baseB * factor;
      break;
    }

    case STYLE_BLINKING:
      if ((millis() / 300) % 2 != 0) {
        currentR = 0; currentG = 0; currentB = 0;
      }
      break;

    case STYLE_RANDOM:
      currentR = random(50, 256);
      currentG = random(50, 256);
      currentB = random(50, 256);
      break;
  }

  setLocalLEDColor(currentR, currentG, currentB);

  for (int i = 0; i < num_leds; i++) {
    if (currentStyle == STYLE_RANDOM) {
      RGBpayload[i * 3 + 0] = random(50, 256);
      RGBpayload[i * 3 + 1] = random(50, 256);
      RGBpayload[i * 3 + 2] = random(50, 256);
    } else {
      RGBpayload[i * 3 + 0] = currentR;
      RGBpayload[i * 3 + 1] = currentG;
      RGBpayload[i * 3 + 2] = currentB;
    }
  }

  bool published = client.publish(mqtt_topic.c_str(), RGBpayload, payload_size);
  if (published) {
    Serial.println("MQTT payload published and local LED updated.");
  } else {
    Serial.println("MQTT publish failed!");
  }
}

void setup() {
  Serial.begin(115200);
  unsigned long startTime = millis();
  while (!Serial && millis() - startTime < 5000) {}

  WiFiDrv::pinMode(25, OUTPUT);
  WiFiDrv::pinMode(26, OUTPUT);
  WiFiDrv::pinMode(27, OUTPUT);

  if (!veml1.begin()) {
    Serial.println("Couldn't find left VEML7700 sensor!");
  } else {
    Serial.println("Connected to the left VEML7700 sensor!");
    veml1.setGain(VEML7700_GAIN_1);
    veml1.setIntegrationTime(VEML7700_IT_100MS);
  }

  pinMode(SOFT_SDA, INPUT_PULLUP);
  pinMode(SOFT_SCL, INPUT_PULLUP);

  memset(RGBpayload, 0, sizeof(RGBpayload));
  startWifi();
  client.setServer(MQTT_BROKER, MQTT_PORT);

  if (!client.setBufferSize(256)) {
    Serial.println("Failed to configure MQTT buffer!");
  }
  reconnectMQTT();
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  if (WiFi.status() != WL_CONNECTED) {
    startWifi();
  }
  client.loop();

  float lux1 = veml1.readLux();
  bool currentRightBlocked = (digitalRead(SOFT_SDA) == LOW);
  bool currentLeftBlocked = (lux1 < LIGHT_THRESHOLD);

  // Check the sensors and determine the gesture input
  if (currentLeftBlocked && !leftCovered) {
    leftCovered = true;
    leftCoverTime = millis();
  }

  // Gesture detection for smooth movement
  if (leftCovered && currentRightBlocked) {
    if (millis() - leftCoverTime < SWIPE_TIMEOUT) {
      currentStyle = (LightStyle)((currentStyle + 1) % 4);
      Serial.print("---->>>>The user is swiping to the right, changing the lighting style to: ");
      Serial.println(currentStyle);
      leftCovered = false;
      delay(400);
    }
  }


  if (leftCovered && (millis() - leftCoverTime > SWIPE_TIMEOUT)) {
    if (!currentRightBlocked) {
      colorIndex = (colorIndex + 1) % totalColors;
      Serial.print("---- Only the left sensor is covered: ");
      Serial.println(colorIndex);
      delay(300);
    }
    leftCovered = false;
  }

  updateAndPublishLights();

  delay(100); 
}