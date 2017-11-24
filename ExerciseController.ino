
  
  // gamecube controller adapter
// This requires this branch of the Arduino STM32 board: 
//    https://github.com/arpruss/Arduino_STM32/tree/addMidiHID
// Software steps:
//    Install this bootloader: https://github.com/rogerclarkmelbourne/STM32duino-bootloader/blob/master/binaries/generic_boot20_pb12.bin?raw=true
//    Instructions here: http://wiki.stm32duino.com/index.php?title=Burning_the_bootloader#Flashing_the_bootloader_onto_the_Black_Pill_via_UART_using_a_Windows_machine/  
//    Install official Arduino Zero board
//    Put the contents of the above branch in your Arduino/Hardware folder
//    If on Windows, run drivers\win\install_drivers.bat
// Note: You may need to adjust permissions on some of the dll, exe and bat files.
// Install this library:
//    https://github.com/arpruss/GameControllersSTM32

// Facing GameCube socket (as on console), flat on top:
//    123
//    ===
//    456

// Connections for GameCube adapter:
// GameCube 2--PA6
// GameCube 2--1Kohm--3.3V
// GameCube 3--GND
// GameCube 4--GND
// GameCube 6--3.3V
// Put a 10 uF and 0.1 uF capacitor between 3.3V and GND right on the black pill board.
// optional: connect GameCube 1--5V (rumble, make sure there is enough current)

// Put LEDs + resistors (100-220 ohm) between PA0,PA1,PA2,PA3 and 3.3V
// Put momentary pushbuttons between PA4 (decrement),PA5 (increment) and 3.3V

// Connections for elliptical/bike:
// GND--GND
// PA7--4.7Kohm--rotation detector [the resistor may not be needed]
// PA7--100Kohm--3.3V

// For direction switch, to tell the device if you're pedaling backwards or forwards
// PA8 - direction control switch, one side
// 3.3V - direction control switch, other side

// Connections for Nunchuck
// GND--GND
// 3.3V--3.3V
// PB6--10Kohm--3.3V
// PB7--10Kohm--3.3V
// PB6--SCL
// PB7--SDA



// Make sure there are 4.7Kohm? pullups for the two i2c data lines

#include <libmaple/iwdg.h>
#include <libmaple/usb_cdcacm.h>
#include <libmaple/usb.h>
#include "GameControllers.h"
#include "ExerciseController.h"

DigitalOutput led(PIN_LED);
Debouncer debounceDown(PIN_BUTTON_DOWN, HIGH);
Debouncer debounceUp(PIN_BUTTON_UP, HIGH);
#ifdef ENABLE_NUNCHUCK
NunchuckController nunchuck = NunchuckController(PIN_NUNCHUCK_SCL,PIN_NUNCHUCK_SDA);
#endif
#ifdef ENABLE_GAMECUBE
GameCubeController gameCube = GameCubeController(PIN_GAMECUBE);
#endif
#ifdef ENABLE_GAMEPORT
GamePortController gamePort = GamePortController(PIN_GAMEPORT_X,PIN_GAMEPORT_Y,PIN_GAMEPORT_SLIDER,PIN_GAMEPORT_RX,
    PIN_GAMEPORT_B1, PIN_GAMEPORT_B2, PIN_GAMEPORT_B3, PIN_GAMEPORT_B4);
#endif

void displayNumber(uint8_t x) {
}

void updateDisplay() {
}

void setup() {
  pinMode(PIN_BUTTON_DOWN, INPUT_PULLDOWN);
  pinMode(PIN_BUTTON_UP, INPUT_PULLDOWN);
  
#ifdef SERIAL_DEBUG
  Serial.begin(115200);
  delay(4000);
  Serial.println("gamecube controller adapter");
#else
  Joystick.setManualReportMode(true);
#endif

  ellipticalInit();

#ifdef ENABLE_GAMECUBE
  gameCube.begin();
#endif
#ifdef ENABLE_NUNCHUCK
  nunchuck.begin();
#endif  
#ifdef ENABLE_GAMECUBE
  gamePort.begin();
#endif

  debounceDown.begin();
  debounceUp.begin();

  pinMode(PIN_LED, OUTPUT);

  EEPROM8_init();
  int i = EEPROM8_getValue(EEPROM_VARIABLE_INJECTION_MODE);
  if (i < 0)
    injectionMode = 0;
  else
    injectionMode = i; 

  if (injectionMode > numInjectionModes)
    injectionMode = 0;

  savedInjectionMode = injectionMode;
  
  updateDisplay();

  lastChangedModeTime = 0;
  iwdg_init(IWDG_PRE_256, watchdogSeconds*156);
}

void updateLED(void) {
  if (((validDevice != CONTROLLER_NONE) ^ ellipticalRotationDetector) && validUSB) {
    led.write(0);
  }
  else {
    led.write(1);
  }
}

uint8_t receiveReport(GameControllerData_t* data) {
  uint8_t success;

#ifdef ENABLE_GAMECUBE
  if (validDevice == CONTROLLER_GAMECUBE || validDevice == CONTROLLER_NONE) {

    success = gameCube.read(data);
    if (success) {
      validDevice = CONTROLLER_GAMECUBE;
      return 1;
    }
    validDevice = CONTROLLER_NONE;
  }
#endif
#ifdef ENABLE_NUNCHUCK
  if (validDevice == CONTROLLER_NUNCHUCK || (validDevice == CONTROLLER_NONE && nunchuck.begin()) ) {
    success = nunchuck.read(data);
    if (success) {
      validDevice = CONTROLLER_NUNCHUCK;
      return 1;
    }
    validDevice = CONTROLLER_NONE;
  }
#endif
#ifdef ENABLE_GAMEPORT
    success = gamePort.read(data);
    if (success) {
      validDevice = CONTROLLER_GAMEPORT;
      return 1;
    }
  }
#endif

  validDevice = CONTROLLER_NONE;

  data->joystickX = 515;
  data->joystickY = 512;
  data->cX = 512;
  data->cY = 512;
  data->buttons = 0;
  data->shoulderLeft = 0;
  data->shoulderRight = 0;
  data->device = CONTROLLER_NONE;

  return 0;
}

void loop() {
  GameControllerData_t data;
  EllipticalData_t elliptical;

  iwdg_feed();
  
  uint32_t t0 = millis();
  while (debounceDown.getRawState() && debounceUp.getRawState() && (millis()-t0)<5000)
        updateLED();
  
  iwdg_feed();

  if ((millis()-t0)>=5000) {
    // TODO: message
    injectionMode = 0;
    EEPROM8_reset();
    savedInjectionMode = 0;
  }
  else {
    t0 = millis();
    do {
      if (debounceDown.wasReleased()) {
        if (injectionMode == 0)
          injectionMode = numInjectionModes-1;
        else
          injectionMode--;
        lastChangedModeTime = millis();
        updateDisplay();
      }
      
      if (debounceUp.wasPressed()) {
        injectionMode = (injectionMode+1) % numInjectionModes;
        lastChangedModeTime = millis();
        updateDisplay();
  #ifdef SERIAL_DEBUG      
        Serial.println("Changed to "+String(injectionMode));
  #endif
      }
      updateLED();
    } while((millis()-t0) < 6);
  }

  ellipticalUpdate(&elliptical);
      
  if (savedInjectionMode != injectionMode && (millis()-lastChangedModeTime) >= saveInjectionModeAfterMillis) {
#ifdef SERIAL_DEBUG
    Serial.println("Need to store");
#endif
    EEPROM8_storeValue(EEPROM_VARIABLE_INJECTION_MODE, injectionMode);
    savedInjectionMode = injectionMode;
  }

#ifndef SERIAL_DEBUG
  if (!usb_is_connected(USBLIB) || !usb_is_configured(USBLIB)) {
    // we're disconnected; save power by not talking to controller
    validUSB = 0;
    updateLED();
    return;
  } // TODO: fix library so it doesn't send on a disconnected connection; currently, we're relying on the watchdog reset 
  else {
    validUSB = 1;
  }
    // if a disconnection happens at the wrong time
#else
  validUSB = 1;
#endif

  receiveReport(&data);
#ifdef SERIAL_DEBUG
  Serial.println("buttons1 = "+String(data.buttons,HEX));  
  Serial.println("joystick = "+String(data.joystickX)+","+String(data.joystickY));  
  Serial.println("c-stick = "+String(data.cX)+","+String(data.cY));  
  Serial.println("shoulders = "+String(data.shoulderLeft)+","+String(data.shoulderRight));      
#else
if (usb_is_connected(USBLIB) && usb_is_configured(USBLIB)) 
    inject(injectors + injectionMode, &data, &elliptical);
#endif
    
  updateLED();
}

