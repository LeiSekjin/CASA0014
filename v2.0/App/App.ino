
#include <Wire.h>
#include <Adafruit_VEML7700.h>
#include <BH1750.h>

#include <WiFiNINA.h>
#include <utility/wifi_drv.h>
#include <PubSubClient.h>
#include <math.h>


// Type "true" to connect to the MQTT light
const bool ENABLE_MQTT = true;

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


unsigned long lastWifiAttempt = 0;
unsigned long lastMQTTAttempt = 0;

const unsigned long WIFI_RETRY_INTERVAL = 10000;
const unsigned long MQTT_RETRY_INTERVAL = 5000;


Adafruit_VEML7700 veml1 =
  Adafruit_VEML7700();
BH1750 bh1750(0x23);

bool vemlReady = false;
bool bh1750Ready = false;
const int BUTTON_PIN = 5;
const int BUTTON_ACTIVE = HIGH;
bool lightOn = true;

enum LightStyle {
  STYLE_CONSTANT,
  STYLE_BREATHING,
  STYLE_BLINKING,
  STYLE_RANDOM
};

LightStyle currentStyle = STYLE_CONSTANT;

const byte COLORS[][3] = {
  {255, 0, 0},
  {253, 194, 194},
  {0, 255, 0},
  {22, 221, 210},
  {165, 55, 13},
  {87, 198, 255},
  {128, 15, 92}
};

int colorIndex = 0;
const int totalColors = 7;

const float LIGHT_THRESHOLD = 20.0;

bool leftWasCovered = false;

unsigned long buttonPressStartTime = 0;

bool buttonIsPressed = false;

const unsigned long LONG_PRESS_TIME = 800;

bool longPressHandled = false;


const int LED_RED = 26;
const int LED_GREEN = 25;
const int LED_BLUE = 27;

void setLocalLEDColor(int r, int g, int b) {

  r = constrain(r, 0, 255);
  g = constrain(g, 0, 255);
  b = constrain(b, 0, 255);

  WiFiDrv::analogWrite(LED_RED, r);
  WiFiDrv::analogWrite(LED_GREEN, g);
  WiFiDrv::analogWrite(LED_BLUE, b);
}

void runRGBSelfTest() {

  Serial.println();
  Serial.println("--------- RGB LED self-inspection ---------");

  Serial.println(" RED");
  setLocalLEDColor(255, 0, 0);
  delay(500);

  Serial.println(" GREEN");
  setLocalLEDColor(0, 255, 0);
  delay(500);

  Serial.println(" BLUE");
  setLocalLEDColor(0, 0, 255);
  delay(500);

  Serial.println(" WHITE");
  setLocalLEDColor(255, 255, 255);
  delay(500);

  Serial.println("switch off RGB LED");
  setLocalLEDColor(0, 0, 0);
  delay(300);

  Serial.println("RGB LED self-inspection finished。");
  Serial.println("-----------------");
}


void startWifi() {

  if (!ENABLE_MQTT) return;

  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  lastWifiAttempt = millis();
}



void reconnectMQTT() {

  if (!ENABLE_MQTT) return;

  if (WiFi.status() != WL_CONNECTED) return;

  if (client.connected()) return;

  if (millis() - lastMQTTAttempt < MQTT_RETRY_INTERVAL) {
    return;
  }

  lastMQTTAttempt = millis();

  Serial.print("Connecting to MQTT...");

  String clientId =
    "MKR1010-CASA0014-" + lightId;

  bool connected = client.connect(
    clientId.c_str(),
    MQTT_USERNAME,
    MQTT_PASSWORD
  );

  if (connected) {

    Serial.println(" connected!");

  } else {

    Serial.print(" failed, state = ");
    Serial.println(client.state());

  }
}

// new BH1750 model
float readRightSensorLux() {

  if (!bh1750Ready) {
    return 50.0;
  }

  float lux = bh1750.readLightLevel();

  if (lux < 0 || !isfinite(lux)) {
    return 50.0;
  }

  return lux;
}


void handleButtonAndSensors() {
  int btnState = digitalRead(BUTTON_PIN);

  if (btnState == BUTTON_ACTIVE && !buttonIsPressed) {

    buttonIsPressed = true;

    buttonPressStartTime = millis();

    longPressHandled = false;

  }

  else if (btnState == BUTTON_ACTIVE && buttonIsPressed) {

    if (!longPressHandled &&
        millis() - buttonPressStartTime >= LONG_PRESS_TIME) {

      currentStyle =
        (LightStyle)((currentStyle + 1) % 4);

      Serial.print(">>> [Long_press] light_style: ");
      Serial.println(currentStyle);

      longPressHandled = true;
    }
  }

  else if (btnState != BUTTON_ACTIVE && buttonIsPressed) {

    unsigned long pressDuration =
      millis() - buttonPressStartTime;

    if (!longPressHandled &&
        pressDuration < LONG_PRESS_TIME &&
        pressDuration > 50) {

      lightOn = !lightOn;

      Serial.print(">>> [Click] light switch: ");

      Serial.println(lightOn ? "On" : "Off");
    }

    buttonIsPressed = false;
  }


// cover the VEML 7700 to change the color
  if (vemlReady) {

    float luxLeft = veml1.readLux();

    if (luxLeft >= 0 && isfinite(luxLeft)) {

      bool leftCurrentlyCovered =
        (luxLeft < LIGHT_THRESHOLD);

      if (leftCurrentlyCovered && !leftWasCovered) {

        colorIndex =
          (colorIndex + 1) % totalColors;

        Serial.print("--- covered: change the color to ");
        Serial.println(colorIndex);

      }

      leftWasCovered = leftCurrentlyCovered;
    }
  }


  //  BH1750：detect the intensity of ambient light
  float luxRight = readRightSensorLux();
  float brightness =
    constrain(luxRight / 150.0, 0.15, 1.0);
  byte baseR = COLORS[colorIndex][0];
  byte baseG = COLORS[colorIndex][1];
  byte baseB = COLORS[colorIndex][2];

  byte finalR = 0;
  byte finalG = 0;
  byte finalB = 0;


// core mode of the RGB light
  if (lightOn) {

    switch (currentStyle) {

      // constant
      case STYLE_CONSTANT:

        finalR = baseR;
        finalG = baseG;
        finalB = baseB;

        break;

      // breath
      case STYLE_BREATHING: {

        float factor =
          (sin(millis() / 300.0) + 1.0) / 2.0;

        finalR = baseR * factor;
        finalG = baseG * factor;
        finalB = baseB * factor;

        break;
      }

      // blink
      case STYLE_BLINKING:

        if ((millis() / 300) % 2 != 0) {

          finalR = 0;
          finalG = 0;
          finalB = 0;

        } else {

          finalR = baseR;
          finalG = baseG;
          finalB = baseB;
        }

        break;

      // random color
      case STYLE_RANDOM:

        finalR = random(50, 256);
        finalG = random(50, 256);
        finalB = random(50, 256);

        break;
    }

    // ambient light control
    finalR *= brightness;
    finalG *= brightness;
    finalB *= brightness;
  }



  setLocalLEDColor(finalR, finalG, finalB);


  for (int i = 0; i < num_leds; i++) {

    if (lightOn && currentStyle == STYLE_RANDOM) {

      RGBpayload[i * 3 + 0] =
        random(50, 256) * brightness;

      RGBpayload[i * 3 + 1] =
        random(50, 256) * brightness;

      RGBpayload[i * 3 + 2] =
        random(50, 256) * brightness;

    } else {

      RGBpayload[i * 3 + 0] = finalR;
      RGBpayload[i * 3 + 1] = finalG;
      RGBpayload[i * 3 + 2] = finalB;

    }
  }


  if (ENABLE_MQTT && client.connected()) {

    client.publish(
      mqtt_topic.c_str(),
      RGBpayload,
      payload_size
    );
  }
}


void setup() {

  Serial.begin(115200);

  unsigned long startTime = millis();

  while (!Serial && millis() - startTime < 5000) {
  }

  Serial.println();
  Serial.println("------------------------------------");
  Serial.println(" MKR WiFi 1010 light control system");
  Serial.println(" VEML7700 + BH1750");
  Serial.println("------------------------------------");


  WiFiDrv::wifiDriverInit();

  WiFiDrv::pinMode(LED_RED, OUTPUT);
  WiFiDrv::pinMode(LED_GREEN, OUTPUT);
  WiFiDrv::pinMode(LED_BLUE, OUTPUT);

  runRGBSelfTest();
  pinMode(BUTTON_PIN, INPUT);
  Wire.begin();
  Wire.setClock(100000);
  
  Serial.println("Initialize VEML7700...");

  if (veml1.begin()) {

    vemlReady = true;

    veml1.setGain(VEML7700_GAIN_1);
    veml1.setIntegrationTime(VEML7700_IT_100MS);

    Serial.println("[SUCCESSFUL] VEML7700 READY");

  } else {

    Serial.println("[FAILED] VEML7700 no connected");
  }

  Serial.println("preparing BH1750...");

  bh1750Ready = bh1750.begin(
    BH1750::CONTINUOUS_HIGH_RES_MODE,
    0x23,
    &Wire
  );

  if (bh1750Ready) {

    Serial.println("[SUCCESSFUL] BH1750 READY");

  } else {

    Serial.println("[FAILED] BH1750 no connected");
  }


  randomSeed(analogRead(A0));

  memset(RGBpayload, 0, sizeof(RGBpayload));

  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setBufferSize(256);

  if (ENABLE_MQTT) {

    Serial.println("MQTT enabled");

    startWifi();

  } else {

    Serial.println("Now it is RGB testing mode");
    Serial.println("Wi-Fi / MQTT closed");
  }


  Serial.println();
  Serial.println("--------- System initialization result ---------");

  Serial.print("VEML7700: ");
  Serial.println(vemlReady ? "READY" : "FAILED");

  Serial.print("BH1750: ");
  Serial.println(bh1750Ready ? "READY" : "FAILED");

  Serial.println("RGB LED: self-inspection ");

  Serial.println("------------------");
}


void loop() {

  if (ENABLE_MQTT) {

    if (WiFi.status() != WL_CONNECTED) {

      if (millis() - lastWifiAttempt >= WIFI_RETRY_INTERVAL) {
        startWifi();
      }

    } else {

      reconnectMQTT();

      if (client.connected()) {
        client.loop();
      }
    }
  }

  handleButtonAndSensors();

  delay(100);
}
