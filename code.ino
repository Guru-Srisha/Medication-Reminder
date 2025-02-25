#define DISABLE_SERIAL_DEBUG
#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME ""

#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include <BlynkSimpleEsp32.h>
#include <EEPROM.h>

RTC_DS3231 rtc;
DateTime now;

// Wi-Fi credentials
const char* ssid = "";
const char* password = "";
const char* apiKey = "";
char auth[] = "";

// SDA:21 SCL:22
const int buzzerPin = 23;
const int morningServoPin = 25;
const int noonServoPin = 26;
const int nightServoPin = 27;
const int buttonPin = 15;
const int snoozePin = 16; // RX2
const int wifiCheckPin = 32;
const int closeButtonPin = 14;
const int resetPin = 33;
const int timeSetPin = 17; //TX2
const int buttonAPin = 18;
const int buttonBPin = 19;
const int buttonCPin = 4;

Servo morningServo;
Servo noonServo;
Servo nightServo;

LiquidCrystal_I2C lcd(0x27, 20, 4);

WiFiClient client;

bool alarmActive = false;
bool snoozeActive = false;
bool resetPromptActive = false;
unsigned long snoozeEndTime = 0;
bool morningAlarmTriggered = false;
bool noonAlarmTriggered = false;
bool nightAlarmTriggered = false;
bool settingTime = false;
bool notChosen = true;
int selectedReminder = 0; 

// Reminder times
int morningHour = 11;
int morningMinute = 53;
int noonHour = 11;
int noonMinute = 55;
int nightHour = 11;
int nightMinute = 57;

unsigned long lastRequestTime = 0;
const unsigned long requestInterval = 10000;
bool wifiDisconnectedOnce = false;
bool rtcUpdated = false;

int morning_count = 6;
int noon_count = 6;
int night_count = 6;

const int morningCounterAddress = 0;
const int noonCounterAddress = 4;
const int nightCounterAddress = 8;
const int morningTimeAddress = 12;
const int noonTimeAddress = 16;
const int nightTimeAddress = 20;

void setup() {
  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(snoozePin, INPUT_PULLUP);
  pinMode(wifiCheckPin, INPUT_PULLUP);
  pinMode(closeButtonPin, INPUT_PULLUP);
  pinMode(resetPin, INPUT_PULLUP);
  pinMode(timeSetPin, INPUT_PULLUP);
  pinMode(buttonAPin, INPUT_PULLUP);
  pinMode(buttonBPin, INPUT_PULLUP);
  pinMode(buttonCPin, INPUT_PULLUP);

  morningServo.attach(morningServoPin);
  noonServo.attach(noonServoPin);
  nightServo.attach(nightServoPin);

  morningServo.write(30);
  noonServo.write(30);
  nightServo.write(30);

  Serial.begin(115200);
  Wire.begin(21, 22);

  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

  if (!rtc.begin()) {
    lcd.setCursor(0, 1);
    lcd.print("RTC not found       ");
    while (1);
  }

  if (rtc.lostPower()) {
    lcd.setCursor(0, 1);
    lcd.print("RTC lost power      ");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  lcd.clear();
  connectToWiFi();

  EEPROM.begin(12);
  EEPROM.writeInt(morningCounterAddress, morning_count);
  EEPROM.writeInt(noonCounterAddress, noon_count);
  EEPROM.writeInt(nightCounterAddress, night_count);
  EEPROM.writeInt(morningTimeAddress, morningHour);
  EEPROM.writeInt(morningTimeAddress + 2, morningMinute);
  EEPROM.writeInt(noonTimeAddress, noonHour);
  EEPROM.writeInt(noonTimeAddress + 2, noonMinute);
  EEPROM.writeInt(nightTimeAddress, nightHour);
  EEPROM.writeInt(nightTimeAddress + 2, nightMinute);
  EEPROM.commit();

  Blynk.begin(auth, ssid, password);
}

void loop() {
  now = rtc.now();

  if (millis() - lastRequestTime >= 10000) {
    printTime();
    lastRequestTime = millis();
  }

  lcd.setCursor(0, 0);
  lcd.print("Time: ");
  lcd.print(now.hour());
  lcd.print(":");
  if (now.minute() < 10) lcd.print("0");
  lcd.print(now.minute());
  lcd.print(" ");
  lcd.print(WiFi.status() == WL_CONNECTED ? "API  " : "RTC  ");

  if (snoozeActive) {
    lcd.setCursor(0, 1);
    lcd.print("On Snooze           ");
  }

  if (morning_count <= 5 || noon_count <= 5 || night_count <= 5) {
    lcd.setCursor(0, 3);
    lcd.print("Low Medicines       ");
  }
  else {
    lcd.setCursor(0, 3);
    lcd.print("                    ");
  }

  if (digitalRead(timeSetPin) == LOW) {
    if (!settingTime) {
        settingTime = true;
        notChosen = true;
        lcd.setCursor(0, 1);
        lcd.print("Set Reminder Time:  ");
        lcd.setCursor(0, 2);
        lcd.print("A:Mo B:No C:Ni      ");
    } else {
        saveReminderTimes();
        settingTime = false;
        lcd.setCursor(0, 1);
        lcd.print("Settings Saved!     ");
        delay(3000);
        lcd.setCursor(0, 1);
        lcd.print("Get Well Soon!      ");
        lcd.setCursor(0, 2);
        lcd.print("                    ");
    }
    delay(300);
}

if (settingTime && notChosen) {
    if (digitalRead(buttonAPin) == LOW) {
        selectedReminder = 0;
        notChosen = false;
        displayReminderTime(morningHour, morningMinute);
    } else if (digitalRead(buttonBPin) == LOW) {
        selectedReminder = 1;
        notChosen = false;
        displayReminderTime(noonHour, noonMinute);
    } else if (digitalRead(buttonCPin) == LOW) {
        selectedReminder = 2;
        notChosen = false;
        displayReminderTime(nightHour, nightMinute);
    }
}

if (!notChosen && settingTime) {
    if (digitalRead(buttonAPin) == LOW) {
        adjustReminderTimeBy(selectedReminder, -1);
        delay(300);
    }
    if (digitalRead(buttonCPin) == LOW) {
        adjustReminderTimeBy(selectedReminder, 1);
        delay(300);
    }
}

  if (digitalRead(buttonPin) == LOW && (settingTime || resetPromptActive)) {
    settingTime = false;
    resetPromptActive = false;
    notChosen == false;
    lcd.setCursor(0, 1);
    lcd.print("Operation Discarded ");
    lcd.setCursor(0, 2);
    lcd.print("                    ");
    delay(3000);
    lcd.setCursor(0, 1);
    lcd.print("Get Well Soon!      ");
  }

  if (digitalRead(resetPin) == LOW) {
    if (resetPromptActive) {
        resetCountersAndServos();
        resetPromptActive = false;
        lcd.setCursor(0, 1);
        lcd.print("Dont forget to close");
        lcd.setCursor(0, 2);
        lcd.print("Count Reset!        ");
        delay(3000);
        lcd.setCursor(0, 2);
        lcd.print("                    ");
    } else {
        resetPromptActive = true;
        lcd.setCursor(0, 2);
        lcd.print("Press Again         ");
    }
    delay(300);
  }

  if (digitalRead(wifiCheckPin) == LOW) {
    Serial.println("Wi-Fi Check button pressed");
    connectToWiFi();
    delay(300);
  }

  if (digitalRead(closeButtonPin) == LOW) {
    morningServo.write(30);
    noonServo.write(30);
    nightServo.write(30);
    Serial.println("Close button pressed");
    lcd.setCursor(0, 1);
    lcd.print("Get Well Soon!      ");
    delay(300);
  }

  if (digitalRead(snoozePin) == LOW) {
    if (alarmActive) {
      deactivateAlarm(false);
      snoozeActive = true;
      snoozeEndTime = millis() + 60000;
      morningServo.write(30);
      noonServo.write(30);
      nightServo.write(30);
      Serial.println("Snooze button pressed");
    }
    delay(300);
  }

  if (digitalRead(buttonPin) == LOW) {
    if (alarmActive) {
        if (morningAlarmTriggered && morning_count > 0) {
            morning_count--;
            EEPROM.writeInt(morningCounterAddress, morning_count);
            EEPROM.commit();
            Serial.println("Morning counter decremented.");
        } else if (noonAlarmTriggered && noon_count > 0) {
            noon_count--;
            EEPROM.writeInt(noonCounterAddress, noon_count);
            EEPROM.commit();
            Serial.println("Noon counter decremented.");
        } else if (nightAlarmTriggered && night_count > 0) {
            night_count--;
            EEPROM.writeInt(nightCounterAddress, night_count);
            EEPROM.commit();
            Serial.println("Night counter decremented.");
        }
        deactivateAlarm(false);
    }

    Serial.println("Main button pressed");
    delay(300);
  }

  if (snoozeActive && millis() > snoozeEndTime) {
    snoozeActive = false;
    activateAlarm();
    if (morningAlarmTriggered) {
      morningServo.write(180);
      Serial.println("Morning alarm reactivated, servo moved to 180 degrees");
    } else if (noonAlarmTriggered) {
      noonServo.write(180);
      Serial.println("Noon alarm reactivated, servo moved to 180 degrees");
    } else if (nightAlarmTriggered) {
      nightServo.write(180);
      Serial.println("Night alarm reactivated, servo moved to 180 degrees");
    }
  }

  if (alarmActive) {
    if (morningAlarmTriggered) {
      lcd.setCursor(0, 1);
      lcd.print("Morning Medicines   ");
      Blynk.logEvent("morning_medicines");
    } else if (noonAlarmTriggered) {
      lcd.setCursor(0, 1);
      lcd.print("Noon Medicines      ");
      Blynk.logEvent("noon_medicines");
    } else if (nightAlarmTriggered) {
      lcd.setCursor(0, 1);
      lcd.print("Night Medicines     ");
      Blynk.logEvent("night_medicines");
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    checkAndTriggerAlarm(now.hour(), now.minute());
  } else {
    if (millis() - lastRequestTime >= requestInterval) {
      checkAlarms();
    }
  }

  Blynk.run();
}

BLYNK_WRITE(V0) {
  int value = param.asInt();
  if (value == 1 && alarmActive) {
    deactivateAlarm(false);
    morningServo.write(0);
    noonServo.write(0);
    nightServo.write(0);
    Serial.println("Blynk: Alarm turned off");
  }
}

BLYNK_WRITE(V1) {
  int value = param.asInt();
  if (value == 1 && alarmActive) {
    deactivateAlarm(false);
    snoozeActive = true;
    snoozeEndTime = millis() + 60000;
    morningServo.write(0);
    noonServo.write(0);
    nightServo.write(0);
    Serial.println("Blynk: Snooze activated");
  }
}

void connectToWiFi() {
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, password);
  unsigned long wifiConnectStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiConnectStart < 10000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Wi-Fi");
    wifiDisconnectedOnce = false;
    lcd.setCursor(0, 2);
    lcd.print("Time updated!       ");
    delay(3000);
    lcd.setCursor(0, 2);
    lcd.print("                    ");
  } else {
    Serial.println("\nWi-Fi connection failed.");
  }
}

void checkAlarms() {
  if (millis() - lastRequestTime >= requestInterval) {
    HTTPClient http;
    String url = "http://api.timezonedb.com/v2.1/get-time-zone?key=" + String(apiKey) + "&format=json&by=zone&zone=Asia/Kolkata";
    http.begin(client, url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      if (payload.length() == 0) {
        return;
      }
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        delay(1000);
        return;
      }
      String dateTime = doc["formatted"];
      int year = dateTime.substring(0, 4).toInt();
      int month = dateTime.substring(5, 7).toInt();
      int day = dateTime.substring(8, 10).toInt();
      int hour = dateTime.substring(11, 13).toInt();
      int minute = dateTime.substring(14, 16).toInt();
      int second = dateTime.substring(17, 19).toInt();
      rtc.adjust(DateTime(year, month, day, hour, minute, second));
      if (!rtcUpdated) {
        Serial.println("RTC time updated from API");
        rtcUpdated = true;
      }
    } else {
      Serial.println("Error fetching time from API");
    }
    http.end();
  }
  checkAndTriggerAlarm(now.hour(), now.minute());
}

void checkAndTriggerAlarm(int hour, int minute) {
  if (hour == morningHour && minute == morningMinute && !morningAlarmTriggered) {
    Serial.println("Activating Morning Alarm!");
    resetFlags();
    morningServo.write(180);
    activateAlarm();
    morningAlarmTriggered = true;
  } else if (hour == noonHour && minute == noonMinute && !noonAlarmTriggered) {
    Serial.println("Activating Noon Alarm!");
    resetFlags();
    noonServo.write(180);
    activateAlarm();
    noonAlarmTriggered = true;
  } else if (hour == nightHour && minute == nightMinute && !nightAlarmTriggered) {
    Serial.println("Activating Night Alarm!");
    resetFlags();
    nightServo.write(180);
    activateAlarm();
    nightAlarmTriggered = true;
  }
}

void resetFlags() {
  morningAlarmTriggered = false;
  noonAlarmTriggered = false;
  nightAlarmTriggered = false;
}

void activateAlarm() {
  alarmActive = true;
  digitalWrite(buzzerPin, HIGH);
}

void deactivateAlarm(bool resetFlag) {
    alarmActive = false;
    digitalWrite(buzzerPin, LOW);
    if (resetFlag) resetFlags();

    lcd.setCursor(0, 1);
    lcd.print("                    ");
    lcd.setCursor(0, 2);
    lcd.print("                    ");
    lcd.setCursor(0, 1);
    lcd.print("Morning:");
    lcd.print(morning_count);
    lcd.print(" Noon:");
    lcd.print(noon_count);
    lcd.setCursor(0, 2);
    lcd.print("Night:");
    lcd.print(night_count);

    delay(3000);

    lcd.setCursor(0, 1);
    lcd.print("Get Well Soon!      ");
    lcd.setCursor(0, 2);
    lcd.print("                    ");
}

void resetCountersAndServos() {
  morning_count = 6;
  noon_count = 6;
  night_count = 6;

  EEPROM.writeInt(morningCounterAddress, morning_count);
  EEPROM.writeInt(noonCounterAddress, noon_count);
  EEPROM.writeInt(nightCounterAddress, night_count);
  EEPROM.commit();

  morningServo.write(180);
  noonServo.write(180);
  nightServo.write(180);

  lcd.setCursor(0, 1);
  lcd.print("Get Well Soon!      ");

  Serial.println("Counters reset to 30 and servos set to 180.");
}

void adjustReminderTime() {
  int *hour, *minute;
  switch (selectedReminder) {
    case 0: hour = &morningHour; minute = &morningMinute; break;
    case 1: hour = &noonHour; minute = &noonMinute; break;
    case 2: hour = &nightHour; minute = &nightMinute; break;
  }

  if (digitalRead(buttonAPin) == LOW) {
    *minute -= 1;
    if (*minute < 0) {
      *minute += 60;
      (*hour)--;
    }
    if (*hour < 0) *hour += 24;
    displayReminderTime(*hour, *minute);
    delay(300);
  }

  if (digitalRead(buttonCPin) == LOW) {
    *minute += 1;
    if (*minute >= 60) {
      *minute -= 60;
      (*hour)++;
    }
    if (*hour >= 24) *hour -= 24;
    displayReminderTime(*hour, *minute);
    delay(300);
  }
}

void adjustReminderTimeBy(int reminder, int adjustment) {
    int *hour, *minute;
    switch (reminder) {
        case 0: hour = &morningHour; minute = &morningMinute; break;
        case 1: hour = &noonHour; minute = &noonMinute; break;
        case 2: hour = &nightHour; minute = &nightMinute; break;
    }

    *minute += adjustment;
    if (*minute >= 60) {
        *minute -= 60;
        (*hour)++;
    } else if (*minute < 0) {
        *minute += 60;
        (*hour)--;
    }
    if (*hour >= 24) *hour -= 24;
    if (*hour < 0) *hour += 24;

    displayReminderTime(*hour, *minute);
}


void displayReminderTime(int hour, int minute) {
  lcd.setCursor(0, 1);
  lcd.print("                    ");
  lcd.setCursor(0, 1);
  if (selectedReminder == 0) lcd.print("Morning Reminder:");
  else if (selectedReminder == 1) lcd.print("Noon Reminder:");
  else if (selectedReminder == 2) lcd.print("Night Reminder:");

  lcd.setCursor(0, 2);
  lcd.print("                    ");
  lcd.setCursor(0, 2);
  lcd.print(hour);
  lcd.print(":");
  if (minute < 10) lcd.print("0");
  lcd.print(minute);
}

void saveReminderTimes() {
  EEPROM.writeInt(morningTimeAddress, morningHour);
  EEPROM.writeInt(morningTimeAddress + 2, morningMinute);
  EEPROM.writeInt(noonTimeAddress, noonHour);
  EEPROM.writeInt(noonTimeAddress + 2, noonMinute);
  EEPROM.writeInt(nightTimeAddress, nightHour);
  EEPROM.writeInt(nightTimeAddress + 2, nightMinute);
  EEPROM.commit();
}

void printTime() {
  Serial.print("Current Time: ");
  Serial.print(now.hour());
  Serial.print(":");
  if (now.minute() < 10) Serial.print("0");
  Serial.print(now.minute());
  Serial.print(" ");
  Serial.print(WiFi.status() == WL_CONNECTED ? "(API)" : "(RTC)");
  Serial.println();
  Serial.print("Morning Counter: ");
  Serial.println(morning_count);
  Serial.print("Noon Counter: ");
  Serial.println(noon_count);
  Serial.print("Night Counter: ");
  Serial.println(night_count);
  Serial.print("Morning Time: ");
  Serial.print(morningHour);
  Serial.print(":");
  Serial.println(morningMinute);
  Serial.print("Noon Time: ");
  Serial.print(noonHour);
  Serial.print(":");
  Serial.println(noonMinute);
  Serial.print("Night Time: ");
  Serial.print(nightHour);
  Serial.print(":");
  Serial.println(nightMinute);
}
