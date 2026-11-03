# Dual Light Sensor Lighting Control System: Operating Logic

✔ After extensive testing, v2.0 can now successfully power on and run
the lighting system.

## 1. Sensors and Hardware Components

This project uses an Arduino MKR WiFi 1010 as the main controller. By
combining two different types of light sensors, a button, and MQTT
communication, it enables coordinated control of the onboard RGB LED and
a remote LED light.

-   **Arduino MKR WiFi 1010**: Reads the sensor and button states,
    executes the lighting logic, and communicates with the MQTT Broker
    over Wi-Fi.
-   **Left VEML7700 light sensor**: Detects whether it is being covered.
    When coverage is detected, the system switches the current light
    colour.
-   **Right BH1750 light sensor (💡 a new sensor introduced in v2.0,
    intended to completely avoid I2C issues)**: Measures the ambient
    light intensity and automatically adjusts the light brightness
    accordingly.
-   **Digital Push Button v3**: A short press switches the light on or
    off, while a long press changes the lighting style.
-   **Onboard RGB LED**: Displays the current light colour and effect,
    providing local feedback.
-   **MQTT remote light bulb**: The key demonstration outcome of the
    project.

The two sensors use different I2C addresses. In the code, the VEML7700
is initialised through `Wire`, while the BH1750 uses address `0x23` and
is connected to the same hardware I2C bus.

------------------------------------------------------------------------

## 2. Main Control Logic

### 2.1 Digital Push Button v3 Control

The button is connected to pin **D5** on the development board. The
program treats `HIGH` as the pressed state.

The button supports two operations: **short press** and **long press**.

-   **Short press (pressed for more than 50ms and less than 800ms)**:
    Toggles the `lightOn` state, turning the light on or off.
-   **Long press (held for at least 800ms)**: Switches to the next
    lighting style.

When the long-press duration reaches 800ms, the program switches the
style once immediately and uses `longPressHandled` to mark the long
press as processed. This prevents the style from changing repeatedly
while the button remains held down. When the button is released, its
state is reset and the system waits for the next operation.

The lighting styles cycle in the following order:

1.  `STYLE_CONSTANT`: Constant light
2.  `STYLE_BREATHING`: Breathing
3.  `STYLE_BLINKING`: Blinking
4.  `STYLE_RANDOM`: Random colours

### 2.2 Left VEML7700: Colour Control

**The left VEML7700 is responsible for detecting covering actions.**

The program reads the sensor's illuminance using `veml1.readLux()` and
uses `LIGHT_THRESHOLD = 20.0` as the threshold for detecting coverage:

-   When the illuminance falls below 20 lux, the sensor is considered
    covered.
-   When the sensor changes from an uncovered state to a covered state,
    `colorIndex` is incremented and the next preset colour is selected.
-   If the sensor remains covered, the colour will not keep changing.
-   The obstruction must first be removed so that the sensor returns to
    the uncovered state. Covering it again will then trigger the next
    colour change.

The program uses `leftWasCovered` to record the previous coverage state.
This allows it to detect a state transition instead of triggering the
action repeatedly in every loop iteration.

### 2.3 Right BH1750: Ambient Brightness Control

**The right BH1750 continuously measures the ambient light intensity.**

The program reads the illuminance using `bh1750.readLightLevel()` and
calculates a brightness factor from the reading:

``` cpp
brightness = constrain(luxRight / 150.0, 0.15, 1.0);
```

Its operation is as follows:

-   The lower the ambient illuminance, the lower the calculated
    brightness factor.
-   As the ambient illuminance increases, the brightness factor
    increases accordingly.
-   The brightness factor is limited to a minimum of `0.15` and a
    maximum of `1.0`.
-   When the illuminance reaches or exceeds approximately 150 lux, the
    brightness factor reaches its maximum value of 1.0.

The system therefore adjusts the light output automatically according to
the ambient brightness, rather than always operating at a fixed
brightness.

If the BH1750 fails to initialise, or if its reading is invalid (less
than 0 or not a finite number), `readRightSensorLux()` returns the
default value `50.0`. The program continues running and calculates the
brightness factor using this default illuminance value, rather than
turning the light off directly.

------------------------------------------------------------------------

## 3. Lighting Effects

The program uses three main variables to control the lighting state:
`lightOn`, `currentStyle`, and `colorIndex`.

-   `lightOn`: Determines whether the light is on or off.
-   `currentStyle`: Records the current lighting style.
-   `colorIndex`: Records the currently selected preset colour.

When `lightOn` is `false`, the final RGB output remains at 0, and both
the onboard RGB LED and the remote light are turned off.

When `lightOn` is `true`, the program calculates the RGB output
according to the current lighting style.

### 3.1 Constant Mode (`STYLE_CONSTANT`)

The current preset colour is used directly as the base RGB value, and
each channel is then multiplied by the brightness factor calculated from
the BH1750 reading.

### 3.2 Breathing Mode (`STYLE_BREATHING`)

The program uses a sine function to change the brightness periodically:

``` cpp
factor = (sin(millis() / 300.0) + 1.0) / 2.0;
```

This factor varies periodically between 0 and 1, causing the light to
fade from dim to bright and then from bright to dim, creating a
breathing effect. The result is then multiplied by the ambient
brightness factor.

### 3.3 Blinking Mode (`STYLE_BLINKING`)

The program switches between the on and off states approximately every
300ms based on the elapsed running time:

-   During one time interval, the current colour is output.
-   During the next interval, black is output.

This creates a continuous blinking effect. The ambient brightness factor
also affects the light during the on phase.

### 3.4 Random Colour Mode (`STYLE_RANDOM`)

In random mode, a random value between 50 and 255 is generated
independently for each of the three RGB channels. The values are then
multiplied by the ambient brightness factor to create a random-colour
effect.

------------------------------------------------------------------------

## 4. Lighting Output and Network Communication

### 4.1 Onboard RGB LED Output

The program controls the onboard RGB LED of the MKR WiFi 1010 using
`WiFiDrv::analogWrite()`.

The following pin numbers are used to control the three colour channels:
Red-26, Green-25, Blue-27.

Before the sensors and network connection are initialised, the program
runs `runRGBSelfTest()`. It displays red, green, blue, and white in
sequence, and then turns off the RGB LED. Each test colour is displayed
for approximately 500ms to check whether the onboard RGB LED is working
correctly.

### 4.2 Remote LED Strip Output

The remote light receives lighting data over Wi-Fi and MQTT.

The main configuration items in the program include:

-   Wi-Fi network name and password.
-   MQTT Broker address and port.
-   MQTT username and password.
-   Light identifier `lightId`.
-   MQTT topic and light identifier.

The program writes the RGB data for each LED into `RGBpayload` in
sequence. When the MQTT client is connected, it publishes the data to
the corresponding topic using `client.publish()`.

### 4.3 Wi-Fi and MQTT Reconnection Mechanism

The program uses `ENABLE_MQTT` to determine whether remote lighting
communication is enabled.

-   When `ENABLE_MQTT` is `true`, the system attempts to connect to
    Wi-Fi and then connects to the MQTT Broker once the Wi-Fi connection
    is established.
-   If Wi-Fi is not connected, the program retries the connection
    approximately every 10 seconds.
-   If Wi-Fi is connected but MQTT is not, the program attempts to
    reconnect to MQTT approximately every 5 seconds.
-   Once the MQTT connection is established, the main loop calls
    `client.loop()` to maintain client communication.

If `ENABLE_MQTT` is set to `false`, the program will not start the Wi-Fi
or MQTT connection process. However, the button, sensors, and onboard
RGB LED control logic will continue to operate.
