#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal.h>
#include <LiquidCrystal_I2C.h>

class Alarm {
  private:
    int pin;
  public:
    Alarm(){}

    Alarm(int pin) {
      this->pin = pin;
      pinMode(pin, OUTPUT);
    }

    void beepSound() {
      digitalWrite(pin, HIGH);
    }

    void turnOff() {
       digitalWrite(pin, LOW);
    }
};


class TemperatureSensor {
  private:
    int pin;
    OneWire oneWire;
    DallasTemperature sensors;
  public:
    TemperatureSensor() {}

    TemperatureSensor(int pin) {
      this->pin = pin;

      oneWire = OneWire(pin);
      sensors = DallasTemperature(&oneWire);

      sensors.begin();
    }

  int getTemperature() {
    sensors.requestTemperatures();
    return sensors.getTempCByIndex(0);
  }
};

class Joystick {
  private:
    int xPin;
    int yPin;
    int swPin;

    int deadZoneX = 500;
    int deadZoneY = 500;
    int selectOneZone = 23;

    bool selectedX = false;
    bool selectedY = false;

    void handleSelectionType(int x, int y) {
      if (y < 1023 / 2 + deadZoneY / 2 && y > 1023 / 2 - deadZoneY / 2) {
        selectedY = false;
      } else if (x < 1023 / 2 + deadZoneX / 2 && x > 1023 / 2 - deadZoneX / 2) {
        selectedX = false;
      } else if (y >= 1023 - selectOneZone || y <= 0 + selectOneZone) {
        selectedY = false;
      } else if (x >= 1023 - selectOneZone || x <= 0 + selectOneZone) {
        selectedX = false;
      }
    }

  public:
    String LEFT_MOVEMENT = "L";
    String RIGHT_MOVEMENT = "R";
    String UP_MOVEMENT = "U";
    String DOWN_MOVEMENT = "D";
    String PRESS_MOVEMENT = "P";

    Joystick() {}

    Joystick(int xPin, int yPin, int swPin) {
      this->xPin = xPin;
      this->yPin = yPin;
      this->swPin = swPin;

      pinMode(xPin, INPUT);
      pinMode(yPin, INPUT);
      pinMode(swPin, INPUT_PULLUP);
    }

    String getMovement() {
      int x = analogRead(xPin);
      int y = analogRead(yPin);
      int swState = digitalRead(swPin);

      if (selectedX || selectedY) {
        handleSelectionType(x, y);
        return "";
      }

      if (x > (1023 / 2 + deadZoneX / 2)) {
        if (x < 1023 - selectOneZone) {
          selectedX = true;
        }
        return RIGHT_MOVEMENT;
      }
      if (x < (1023 / 2 - deadZoneX / 2)) {
        if (x > 0 + selectOneZone) {
          selectedX = true;
        }
        return LEFT_MOVEMENT;
      }
      if (y > (1023 / 2 + deadZoneY / 2)) {
        if (y < 1023 - selectOneZone) {
          selectedY = true;
        }
        return DOWN_MOVEMENT;
      }
      if (y < (1023 / 2 - deadZoneY / 2)) {
        if (y > 0 + selectOneZone) {
          selectedY = true;
        }
        return UP_MOVEMENT;
      }
      if (swState == LOW) {
        return PRESS_MOVEMENT;
      }

      return "";
    }
};

class Button {
  private:
    int pressed = 0;
    int state = LOW;
    int lastState = LOW;
    int pin;
  public:
    Button() {}

    Button(byte pin) {
      this->pin = pin;
      pinMode(pin, INPUT);
    }

    void update() {
      int state = digitalRead(pin);

      if (state == HIGH && lastState == LOW) {
        pressed = 1 - pressed;
        delay(20);
      } else {
        pressed = 0;
      }

      lastState = state;
    }

    bool isPressed() {
      update();
      return (pressed == HIGH);
    }
};

class ScreenHandler {
  private:
    LiquidCrystal_I2C screen = LiquidCrystal_I2C(0x27, 20, 4);

  public:
    ScreenHandler() {
    }

    void initializeScreen() {
      screen.init();
      screen.backlight();
    }

    void show(String firstLine, String secondLine, String thirdLine, String forthLine) {
      /*String fillLine = "";
      firstLine += fillLine;
      secondLine += fillLine;
      thirdLine += fillLine;
      forthLine += fillLine;*/

      screen.setCursor(0, 0);
      screen.print(firstLine);
      screen.setCursor(0, 1);
      screen.print(secondLine);
      screen.setCursor(0, 2);
      screen.print(thirdLine);
      screen.setCursor(0, 3);
      screen.print(forthLine);
    }

    void clearAndShow(String firstLine, String secondLine, String thirdLine, String forthLine) {
      screen.clear();
      show(firstLine, secondLine, thirdLine, forthLine);
    }

    void showInCursor(String msg, int row, int col) {
      screen.setCursor(col, row);
      screen.print(msg);
    }
};

class MashProcessor {
  private:
    int selectedMashTemperature = 67;
    int selectedMashTime = 60;
    int selectedRecirculationTime = 15;
    int actualMashTemperature = 0;
    int actualMashTime = 0;
    int actualRecirculationTime = 0;

    long startingTime = 0;
    bool configFinished = false;
    bool processFinished = false;

    int manualModeSwitch = LOW;
    int waterPumpSwitch = LOW;
    int waterPumpHeatSwitch = LOW;

    ScreenHandler screenHandler;
    TemperatureSensor temperatureSensor;
    Joystick joystick;
    Alarm alarm;

    const int CONFIG_TEMP_ID = 1;
    const int CONFIG_TIME_ID = 2;
    int *currentConfig = &selectedMashTemperature;
    int currentConfigToSelect = CONFIG_TEMP_ID;

    bool alarmActivated = false;

    void displayMashSelectionMenu() {
      String mashTemperatureMsg = " temp = " + String(selectedMashTemperature) + " C";
      String mashTimeMsg = " time = " + String(selectedMashTime) + " min";
      screenHandler.clearAndShow("Mashing config", "", mashTemperatureMsg, mashTimeMsg);
    }

    void displayMashSelectionUpdatingValues() {
      String mashTemperatureMsg = String(selectedMashTemperature) + " C";
      String mashTimeMsg = String(selectedMashTime) + " min";
      screenHandler.showInCursor(mashTemperatureMsg, 2, 8);
      screenHandler.showInCursor(mashTimeMsg, 3, 8);
    }

    void displayRecirculationSelectionMenu() {
      String recirculationTimeMsg = " time = " + String(selectedRecirculationTime) + " min";
      screenHandler.clearAndShow("Recirculation config", "", recirculationTimeMsg, "");
    }

    void displayRecirculationSelectionUpdatingValues(){
      String recirculationTimeMsg = String(selectedRecirculationTime) + " min";
      screenHandler.showInCursor(recirculationTimeMsg, 2, 8);
    }

    void displayConfirmConfigMenu() {
      String mashSettingMsg = "Mash set " + String(selectedMashTemperature) + "C " + String(selectedMashTime) + "min";
      String recirculateSettingMsg = "Rercirc set " + String(selectedRecirculationTime) + "min";
      screenHandler.clearAndShow("Confirm settings", "", mashSettingMsg, recirculateSettingMsg);
    }

    void displayMashingMenu() {
      String formattedSelectedTime = selectedMashTime<10? "0" + String(selectedMashTime): String(selectedMashTime);
      String mashingTempMsg = " temp  " + String(selectedMashTemperature) + "C | " + String(actualMashTemperature) + "C";
      String mashingTimeMsg = " time  " + formattedSelectedTime + "m | " + getTimeFormat(actualMashTime);
      screenHandler.clearAndShow("Mashing", "       set | actual", mashingTempMsg, mashingTimeMsg);
    }

    void displayMashingUpdatingValues() {
      String mashingTempMsg = String(actualMashTemperature) + "C";
      String mashingTimeMsg = getTimeFormat(actualMashTime);
      screenHandler.showInCursor(mashingTempMsg, 2, 13);
      screenHandler.showInCursor(mashingTimeMsg, 3, 13);
    }

    void displayRecirculatingMenu() {
      String formattedSelectedTime = selectedRecirculationTime<10? "0" + String(selectedRecirculationTime): String(selectedRecirculationTime);
      String recircTempMsg = " temp      | " + String(actualMashTemperature) + "C"; // FIXME change this variable to one for recirc
      String recircTimeMsg = " time  " + formattedSelectedTime + "m | " + getTimeFormat(actualRecirculationTime);
      screenHandler.clearAndShow("Recirculating", "       set | actual", recircTempMsg, recircTimeMsg);
    }

    void displayRecirculatingUpdatingValues(){
      String recircTempMsg = String(actualMashTemperature) + "C"; // FIXME change this variable to one for recirc
      String recircTimeMsg = getTimeFormat(actualRecirculationTime);
      screenHandler.showInCursor(recircTempMsg, 2, 13);
      screenHandler.showInCursor(recircTimeMsg, 3, 13);
    }

    void displayFinishMenu() {
      screenHandler.clearAndShow("Process finished", "", "Lets cook it!", "");
    }

    String getTimeFormat(int timeInSeg) {
      int min = timeInSeg / 60;
      int seg = timeInSeg - (min * 60);

      String minStr = String(min);
      if (min < 10) {
        minStr = "0" + minStr;
      }

      String segStr = String(seg);
      if (seg < 10) {
        segStr = "0" + segStr;
      }

      return String(minStr) + ":" + String(segStr);
    }

    void beepSound(int max, int interval) {
      for(int i=0; i<max; i++) {
        alarm.beepSound();
        delay(interval);
        alarm.turnOff();
        delay(interval);
      }
    }

    void readMashSettings() {
      String movement = joystick.getMovement();

      if (movement == joystick.RIGHT_MOVEMENT) {
         if (currentConfigToSelect == CONFIG_TEMP_ID) {
          currentConfig = &selectedMashTime;
          currentConfigToSelect = CONFIG_TIME_ID;

         }
         else if (currentConfigToSelect == CONFIG_TIME_ID) {
          currentConfig = &selectedMashTemperature;
          currentConfigToSelect = CONFIG_TEMP_ID;
         }
      }
      else if (movement == joystick.UP_MOVEMENT) {
        *currentConfig = *currentConfig + 1;
        displayMashSelectionUpdatingValues();
      }
      else if (movement == joystick.DOWN_MOVEMENT) {
        *currentConfig = *currentConfig - 1;
        displayMashSelectionUpdatingValues();
      }
    }

  void readRecirculationSettings() {
    String movement = joystick.getMovement();

    if (movement == joystick.UP_MOVEMENT) {
        selectedRecirculationTime++;
        displayRecirculationSelectionUpdatingValues();
      }
      else if (movement == joystick.DOWN_MOVEMENT) {
        selectedRecirculationTime--;
        displayRecirculationSelectionUpdatingValues();
      }
  }

  void processMash() {
    startingTime = millis();
    displayMashingMenu();
    
    while ((actualMashTime / 60) < selectedMashTime) {
      actualMashTime = (millis() - startingTime) / 1000;
      actualMashTemperature = temperatureSensor.getTemperature();

      displayMashingUpdatingValues();

      if (actualMashTemperature < selectedMashTemperature) {
        if (!alarmActivated) {
          alarmActivated = true;
          beepSound(7, 80);
        }

        if (manualModeSwitch == LOW && waterPumpSwitch == HIGH && waterPumpHeatSwitch == HIGH) {
          // recirculates hot water
          // TODO turn on water pump relay
        } else {
          // TODO turn off water pump relay
        }
      } else {
        alarmActivated = false;
      }

      if (manualModeSwitch == HIGH) {
        if (waterPumpSwitch == HIGH) {
          // TODO turn on water pump relay
        } else {
          // TODO turn off water pump relay
        }
      }
    }
  }

  void processRecirculation() {
    startingTime = millis();
    displayRecirculatingMenu();
    
    while ((actualRecirculationTime / 60) < selectedRecirculationTime) {
      actualMashTemperature = temperatureSensor.getTemperature();
      actualRecirculationTime = (millis() - startingTime) / 1000;
      
      displayRecirculatingUpdatingValues();

      if (waterPumpSwitch == LOW) {
        // alarm! water pump is off
        // beepSound(4, 40);
        // TODO turn water pump relay off
      } else {
        // TODO turn water pump relay on
      }
    }
  }

  public:
    MashProcessor() {}
  
    MashProcessor(ScreenHandler screenHandler, TemperatureSensor temperatureSensor, Joystick joystick, Alarm alarm) {
      this->screenHandler = screenHandler;
      this->temperatureSensor = temperatureSensor;
      this->joystick = joystick;
      this->alarm = alarm;
    }

    void config() {
      if (configFinished) {
        return;
      }

      displayMashSelectionMenu();
      while (joystick.getMovement() != joystick.PRESS_MOVEMENT) {
        readMashSettings();
      }

      delay(800);

      displayRecirculationSelectionMenu();
      while (joystick.getMovement() != joystick.PRESS_MOVEMENT) {
        readRecirculationSettings();
      }

      delay(800);

      displayConfirmConfigMenu();
      while (joystick.getMovement() != joystick.PRESS_MOVEMENT) {
      }

      configFinished = true;
    }

    void process() {
      if (processFinished) {
        return;
      }

      processMash();

      processRecirculation();

      processFinished = true;
      displayFinishMenu();
      beepSound(4, 1000);
    }
};

TemperatureSensor temperatureSensor = TemperatureSensor(7);
ScreenHandler screenHandler;
Joystick joystick = Joystick(0, 1, 8);
Alarm alarm = Alarm(13);

MashProcessor* mashProcessor;

String menuItem1 = "1-Mash";
String menuItem2 = "2-Cook";
String menuItem3 = "3-Ferment";
String menuItemSelector = " <--";

void setup() {
  screenHandler.initializeScreen();
  screenHandler.show("    Jarbier 1.0", "", " Lets brew!", "");
  delay(4000);
  mashProcessor = new MashProcessor(screenHandler, temperatureSensor, joystick, alarm);
}

void loop() {
  printMenuOption(1);

  bool optionChanged = false;
  int selectedMenuOption = 1;
  String joystickMovement = "";
  
  while (joystickMovement != joystick.PRESS_MOVEMENT) {
    joystickMovement = joystick.getMovement();
    if (joystickMovement == joystick.DOWN_MOVEMENT) {
      selectedMenuOption++;
      optionChanged = true;
    }
    else if (joystickMovement == joystick.UP_MOVEMENT) {
      selectedMenuOption--;
      optionChanged = true;
    }

    if (selectedMenuOption < 1) {
      selectedMenuOption = 3;
    } else if (selectedMenuOption > 3) {
      selectedMenuOption = 1;
    }

    if (optionChanged) {
      printMenuOption(selectedMenuOption);
      optionChanged = false;
    }
  }

  delay(800);

  if (selectedMenuOption == 1) {
    mashProcessor->config();
    mashProcessor->process();
  } else if (selectedMenuOption == 2) {
    screenHandler.clearAndShow("Beer menu", "", "Option not ready yet", "");
  } else if (selectedMenuOption == 3) {
    screenHandler.clearAndShow("Beer menu", "", "Option not ready yet", "");
  }

  delay(2000);
}

void printMenuOption(int selected) {
  if (selected == 1) {
    screenHandler.clearAndShow("Beer menu", menuItem1 + menuItemSelector, menuItem2, menuItem3);
  } else if (selected == 2) {
    screenHandler.clearAndShow("Beer menu", menuItem1, menuItem2 + menuItemSelector, menuItem3);
  } else if (selected == 3) {
    screenHandler.clearAndShow("Beer menu", menuItem1, menuItem2, menuItem3 + menuItemSelector);
  }
}
