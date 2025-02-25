# Medication Reminder

## Overview
The **Medication Reminder** is a smart medication adherence system designed to help users take their medicines on time. It features automated medicine dispensers, audible alerts, an LCD display, and remote monitoring via Blynk. The system synchronizes time using an API and functions offline using an RTC (DS3231). Additionally, it provides low-supply alerts to ensure timely refills.

## Features
- **Automated Medicine Dispensing:** Uses servos to open medicine compartments at scheduled times.
- **Timely Alerts:** Provides audible and visual reminders to ensure medication adherence.
- **RTC Time Synchronization:** Syncs with an online API when WiFi is available; continues operation offline.
- **Customizable Reminders:** Allows users to set, snooze, or modify reminder schedules.
- **Low-Supply Alerts:** Notifies users when medication levels are running low.
- **Blynk Integration:** Enables remote monitoring and control via a mobile app.
- **User-Friendly Interface:** Features an LCD display and buttons for easy interaction.

## Components
- **ESP32** (Microcontroller)
- **DS3231 RTC Module** (Real-Time Clock)
- **Servo Motors** (For automatic bin opening)
- **LCD Display** (For user interaction)
- **Buzzer** (For alerts)
- **Push Buttons** (For user input)

## Architecture
1. Sturdy hardware interface, designed with 2D, compatible for 5mm Acrylic sheets.
2. The ESP32 controls all hardware components and manages medication schedules.
3. The RTC (DS3231) maintains accurate time, even when WiFi is unavailable.
4. When it's time to take medicine, the system:
   - Activates a buzzer and displays a reminder on the LCD.
   - Automatically opens the corresponding medicine bin.
5. The user acknowledges by pressing a button, which stops the alert and closes the bin.
6. The system tracks medicine consumption and diaplays low-supply alerts on the LCD.
7. Users can set and modify reminders using the hardware interface.
8. Blynk IoT integration allows reminders via notifications and remotely deactivating the reminders.
