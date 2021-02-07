#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal.h>

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

    bool selected = false;

    void handleSelectionTypes(int x, int y) {
      if (y < 1023 / 2 + deadZoneY / 2 && y > 1023 / 2 - deadZoneY / 2) {
        selected = false;
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

      if (selected) {
        handleSelectionTypes(x, y);
        return "";
      }

      if (x > (1023 / 2 + deadZoneX / 2)) {
        if (x < 1023 - selectOneZone) {
          selected = true;
        }
        return RIGHT_MOVEMENT;
      }
      if (x < (1023 / 2 - deadZoneX / 2)) {
        if (x > 0 + selectOneZone) {
          selected = true;
        }
        return LEFT_MOVEMENT;
      }
      if (y > (1023 / 2 + deadZoneY / 2)) {
        if (y < 1023 - selectOneZone) {
          selected = true;
        }
        return DOWN_MOVEMENT;
      }
      if (y < (1023 / 2 - deadZoneY / 2)) {
        if (y > 0 + selectOneZone) {
          selected = true;
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
    LiquidCrystal screen = LiquidCrystal(12, 11, 5, 4, 3, 2);

  public:
    ScreenHandler() {
      screen.begin(16, 2);
    }

    ScreenHandler(int width, int height) {
      screen.begin(width, height);
    }

    void show(String upperMessage, String bottomMessage) {
      upperMessage += "          ";
      bottomMessage += "          ";

      screen.setCursor(0, 0);
      screen.print(upperMessage);
      screen.setCursor(0, 1);
      screen.print(bottomMessage);
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

    const int MENU_ACTUAL_MASHING_DISPLAYED = 1;
    const int MENU_SET_MASHING_DISPLAYED = 2;
    int currentMashingMenu = MENU_ACTUAL_MASHING_DISPLAYED;

    const int CONFIG_TEMP_ID = 1;
    const int CONFIG_TIME_ID = 2;
    int *currentConfig = &selectedMashTemperature;
    int currentConfigToSelect = CONFIG_TEMP_ID;

    bool alarmActivated = false;

    void displayMashSelectionMenu() {
      String mashTemperatureMsg = "T=" + String(selectedMashTemperature) + "C t=" + String(selectedMashTime) + "m";
      screenHandler.show("Mash config", mashTemperatureMsg);
    }

    void displayRecirculationSelectionMenu() {
      String recirculationTimeMsg ="t=" + String(selectedRecirculationTime) + "m";
      screenHandler.show("Recirc. config", recirculationTimeMsg);
    }

    void displayConfirmConfigMenu() {
      String configMsg = "M=" + String(selectedMashTemperature) + "C/" + String(selectedMashTime) + "m R=" + String(selectedRecirculationTime) + "m";
      screenHandler.show("Confirm config", configMsg);
    }

    void displayMashingMenu(int temp, int timeInSeg) {
      String mashMsg = "T=" + String(temp) + "C t=" + getTimeFormat(timeInSeg);
      screenHandler.show("Mashing ...", mashMsg);
    }

    void displayRecirculatingMenu() {
      String recircMsg = "t=" + getTimeFormat(actualRecirculationTime) + " T=" + String(actualMashTemperature);
      screenHandler.show("Recirculating", recircMsg);
    }

    void displayFinishMenu() {
      screenHandler.show("Process finished", "Lets cook it!");
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
      }
      else if (movement == joystick.DOWN_MOVEMENT) {
        *currentConfig = *currentConfig - 1;
      }
    }

  void readRecirculationSettings() {
    String movement = joystick.getMovement();

    if (movement == joystick.UP_MOVEMENT) {
        selectedRecirculationTime++;
      }
      else if (movement == joystick.DOWN_MOVEMENT) {
        selectedRecirculationTime--;
      }
  }

  void processMash() {
    startingTime = millis();

    while ((actualMashTime / 60) < selectedMashTime) {
      actualMashTime = (millis() - startingTime) / 1000;
      actualMashTemperature = temperatureSensor.getTemperature();

      String movement = joystick.getMovement();
      if (currentMashingMenu == MENU_SET_MASHING_DISPLAYED) {
        displayMashingMenu(selectedMashTemperature, selectedMashTime*60);
        if (movement == joystick.UP_MOVEMENT || movement == joystick.DOWN_MOVEMENT) {
          currentMashingMenu = MENU_ACTUAL_MASHING_DISPLAYED;
        }
      } else {
        displayMashingMenu(actualMashTemperature, actualMashTime);
        if (movement == joystick.UP_MOVEMENT || movement == joystick.DOWN_MOVEMENT) {
          currentMashingMenu = MENU_SET_MASHING_DISPLAYED;
        }
      }

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

    while ((actualRecirculationTime / 60) < selectedRecirculationTime) {
      actualMashTemperature = temperatureSensor.getTemperature();
      actualRecirculationTime = (millis() - startingTime) / 1000;
      displayRecirculatingMenu();

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

      while (joystick.getMovement() != joystick.PRESS_MOVEMENT) {
        displayMashSelectionMenu();
        readMashSettings();
      }

      delay(800);

      while (joystick.getMovement() != joystick.PRESS_MOVEMENT) {
        displayRecirculationSelectionMenu();
        readRecirculationSettings();
      }

      delay(800);

      while (joystick.getMovement() != joystick.PRESS_MOVEMENT) {
        displayConfirmConfigMenu();
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
ScreenHandler screenHandler = ScreenHandler(16, 2);
Joystick joystick = Joystick(0, 1, 8);
Alarm alarm = Alarm(13);

MashProcessor mashProcessor = MashProcessor(screenHandler, temperatureSensor, joystick, alarm);

String menuItem1 = "1-Mash";
String menuItem2 = "2-Cook";
String menuItem3 = "3-Ferment";

void setup() {
  screenHandler.show("Jarbier 1.0", "  Lets brew!");
  delay(4000);
}

void loop() {
  screenHandler.show("Beer menu", menuItem1);

  int selectedMenuOption = 1;
  String joystickMovement = "";
  while (joystickMovement != joystick.PRESS_MOVEMENT) {
    joystickMovement = joystick.getMovement();
    if (joystickMovement == joystick.DOWN_MOVEMENT) {
      selectedMenuOption++;
    }
    else if (joystickMovement == joystick.UP_MOVEMENT) {
      selectedMenuOption--;
    }

    if (selectedMenuOption < 1 || selectedMenuOption > 3) {
      selectedMenuOption = 1;
    }

    if (selectedMenuOption == 1) {
      screenHandler.show("Beer menu", menuItem1);
    } else if (selectedMenuOption == 2) {
      screenHandler.show("Beer menu", menuItem2);
    } else if (selectedMenuOption == 3) {
      screenHandler.show("Beer menu", menuItem3);
    }
  }

  delay(800);

  if (selectedMenuOption == 1) {
    mashProcessor.config();
    mashProcessor.process();
  } else if (selectedMenuOption == 2) {
    screenHandler.show("Beer menu", "Not rdy yet");
  } else if (selectedMenuOption == 3) {
    screenHandler.show("Beer menu", "Not rdy yet");
  }

  delay(1000);
}
