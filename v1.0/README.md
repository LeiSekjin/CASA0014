# Dual Light Sensor Lighting Control Logic

## 1. Sensors and Hardware Used

-   **Left VEML7700 light sensor**: Reads ambient illuminance (lux) via
    I2C. When the illuminance falls below the configured threshold
    (currently 20 lux), the program treats the sensor as being covered.
-   **Right VEML7700 light sensor**: Uses a software-emulated I2C
    address. When the light level falls below the threshold, it is
    recognized as a swipe-gesture input to change the lighting mode.
-   **Arduino MKR WiFi 1010**: Reads the sensors, detects user actions,
    and controls the lights.

## 2. Main Logic

The program identifies user actions by detecting the covered states of
the sensors and the order in which they occur:

-   **Cover the left sensor only**: The time when the left sensor is
    first covered is recorded. If the right sensor is not detected as
    covered within the approximately 800 ms detection window, the
    lighting color advances to the next one.
-   **Cover the left sensor first, then the right sensor**: If the right
    sensor becomes covered within approximately 800 ms after the left
    sensor is covered, the action is recognized as a left-to-right
    swipe, and the lighting mode advances to the next one.
-   **Cover the right sensor only**: This does not trigger either of the
    two actions described above.

## 3. Lighting Control

-   **Colors**: Cycles through five colors in order: red, green, blue,
    purple, and cyan.
-   **Modes**: Cycles through four lighting modes in order: constant,
    breathing, blinking, and random color.

## 4. Lighting Output

The Arduino controls two lighting outputs:

-   **On-board RGB LED**: The MKR WiFi 1010 updates its color and
    brightness.
-   **Remote LED strip**: Controlled through an MQTT Broker over Wi-Fi.

## 5. Test Results

-   ✔ Testing confirmed that the Arduino IDE can detect both the left
    and right VEML7700 light sensors.
-   ✔ Covering the left sensor individually changes the lighting color.
-   ❌ Covering the right sensor produces no response, so the swipe
    gesture cannot currently be simulated.
