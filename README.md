ClimateTrack

ESP32-Based IoT Environmental Monitoring System

Overview

ClimateTrack is an embedded IoT environmental monitoring system designed to measure, display, and record temperature and humidity in real time. Built using an ESP32 microcontroller and DHT11 sensor, the system integrates a 16×2 I2C LCD, Wi-Fi connectivity, a live web dashboard, and automated CSV data logging.

The project demonstrates hardware-software integration, embedded C++ programming, wireless communication, and environmental data visualization.

Key Features

* Real-Time Monitoring: Measures temperature and relative humidity approximately every 2.5 seconds using a DHT11 sensor.
* LCD Display: Displays live measurements on a 16×2 LCD using I2C communication.
* Wi-Fi Connectivity: Connects to a local Wi-Fi network or mobile hotspot.
* Web Dashboard: Hosts a browser-accessible monitoring interface directly on the ESP32.
* Live Data Visualization: Displays temperature and humidity history graphs with updates approximately every 3 seconds.
* Automated Data Logging: Records measurements to CSV using LittleFS at approximately 60-second intervals.
* Data Export: Allows downloading recorded measurements for analysis using Excel or Python.

Hardware Components

Component	Purpose
ESP32 Development Board	Microcontroller and Wi-Fi connectivity
DHT11 Sensor	Temperature and humidity measurements
16×2 I2C LCD	Real-time data display
ESP32 Extension Board	Hardware connections
Jumper Wires	Circuit connections
USB Cable	Power and programming

Technologies and Skills

Programming: C++, HTML, CSS, JavaScript

Hardware: ESP32, DHT11, LCD1602, GPIO, I2C

Networking: Wi-Fi, HTTP, JSON

Data Management: LittleFS, CSV Data Logging, Real-Time Data Visualization

Development Tools: Arduino IDE, Linux

System Architecture

The DHT11 collects environmental measurements and transmits them to the ESP32.

The microcontroller processes the measurements and simultaneously supports three functions:

1. Displays readings on the 16×2 LCD.
2. Transmits readings to a local web dashboard using Wi-Fi.
3. Saves measurements in CSV format using LittleFS.

The dashboard displays current temperature and humidity alongside live graphs, allowing users to monitor environmental changes over time.

Getting Started

1. Assemble and connect the DHT11 sensor, ESP32, and LCD according to the wiring documentation.
2. Install the ESP32 board package and required Arduino libraries.
3. Open the ClimateTrack_Live_Graphs.ino sketch in Arduino IDE.
4. Configure the Wi-Fi network name and password locally.
5. Select the correct ESP32 board and serial port.
6. Upload the program.
7. Open Serial Monitor at 115200 baud.
8. Enter the ESP32’s displayed IP address into a browser connected to the same local network.

Electrical note: The ESP32 uses 3.3V GPIO logic. If the LCD’s I2C lines are pulled to 5V, an appropriate bidirectional level shifter is needed.

Data Logging

ClimateTrack stores valid sensor measurements using the ESP32’s LittleFS file system.

Each record contains:

* Time elapsed since system startup
* Temperature in Fahrenheit
* Relative humidity percentage

Records are saved approximately every 60 seconds and can be downloaded as a CSV file.

Project Limitations

* The dashboard operates on the local Wi-Fi network.
* CSV timestamps currently represent elapsed time since startup rather than actual dates.
* Live graphs retain recent browser samples and reset when the webpage is refreshed.
* The DHT11 is suitable for basic environmental monitoring, not precision measurements.

Possible Future Improvements


* Implement configurable temperature and humidity alerts.
* Conduct extended reliability and measurement-accuracy testing.
* Improve historical data visualization.

Project Status

Core functionality completed: Environmental sensing, LCD display, Wi-Fi connectivity, live dashboard, CSV data logging, and live graphs.
