# Wi-Fi Motion Detector (ESP32)

This ESP-IDF project transforms an ESP32 into a motion detector by analyzing real-time Wi-Fi signal strength (RSSI) variations. 

By connecting the ESP32 to a Wi-Fi Access Point (like a Mobile Hotspot), the device continuously polls the signal strength. When a person or object crosses the line-of-sight between the ESP32 and the AP, the RSSI drops. If the signal drops below a configured threshold, the ESP32 triggers an external buzzer to alert of the motion.

## Hardware Required
*   ESP32 Development Board
*   Active Buzzer

## Pinout
By default, the buzzer is connected to:
*   **Buzzer:** GPIO `18`

## Configuration
The project is fully configurable via the ESP-IDF menuconfig.
Run `idf.py menuconfig` and navigate to **Example Configuration** to change:
*   **WiFi SSID & Password:** The credentials for your Wi-Fi AP or Mobile Hotspot.
*   **Buzzer GPIO pin:** The pin connected to your buzzer (default: `18`).
*   **Motion signal threshold (dBm):** The RSSI threshold that triggers the buzzer (default: `-50` dBm). You should tune this value based on the baseline signal strength in your specific room.
*   **Signal check interval (ms):** How often the ESP32 polls the RSSI (default: `200` ms).

## How to Build and Flash
1. Ensure your ESP-IDF environment is set up and activated.
2. Configure your Wi-Fi credentials:
   ```bash
   idf.py menuconfig
   ```
3. Build, flash, and open the serial monitor:
   ```bash
   idf.py build flash monitor
   ```
4. Once connected, watch the serial output to see the real-time RSSI readings and tune your threshold accordingly.

## Security Note
This repository includes a `.gitignore` configured to ignore the `sdkconfig` and `sdkconfig.old` files, ensuring your actual Wi-Fi credentials are never accidentally pushed to version control.
