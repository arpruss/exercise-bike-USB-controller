#include "ExerciseController.h"
#include "GameControllers.h"

#ifndef SERIAL_DEBUG

uint8_t prevButtons[numberOfButtons];
uint8_t curButtons[numberOfButtons];
GameControllerData_t oldData;
bool didJoystick;
const Injector_t* prevInjector = NULL;

void buttonizeStick(uint8_t* buttons, uint16_t x, uint16_t y) {
  int dx = x < 512 ? 512 - x : x - 512;
  int dy = y < 512 ? 512 - y : y - 512;
  
/*  if (dx>directionThreshold || dy>directionThreshold) {
    char msg[40];
    sprintf(msg, "%u,%u %d,%d ", x,y,dx,dy);
    display.display();
    writeText(msg, 0,84, 0);
  } */
  
  if (dx > dy) {
      if(dx > directionThreshold) {
        if (x < 512) {
          buttons[virtualLeft] = 1;
        }
        else {
          buttons[virtualRight] = 1;
        }
      }
  }
  else if (dx < dy && dy > directionThreshold) {
    if (y < 512) {
      buttons[virtualUp] = 1;
    }
    else {
      buttons[virtualDown] = 1;
    }
  }
}

void toButtonArray(uint8_t* buttons, const GameControllerData_t* data) {
  for (int i=0; i<numberOfHardButtons; i++)
    buttons[i] = 0 != (data->buttons & buttonMasks[i]);
  buttons[virtualShoulderRightPartial] = data->shoulderRight>=shoulderThreshold;
  buttons[virtualShoulderLeftPartial] = data->shoulderLeft>=shoulderThreshold;
  buttons[virtualLeft] = buttons[virtualRight] = buttons[virtualDown] = buttons[virtualUp] = 0;
  buttonizeStick(buttons, data->joystickX, data->joystickY);
  if (data->device == CONTROLLER_GAMECUBE)
    buttonizeStick(buttons, data->cX, data->cY); 
}

void joystickBasic(const GameControllerData_t* data) {
    didJoystick = true;
    Joystick.X(data->joystickX);
    Joystick.Y(data->joystickY);
    Joystick.Xrotate(data->cX);
    Joystick.Yrotate(data->cY);
}

void joystickPOV(const GameControllerData_t* data) {
    didJoystick = true;
    int16_t dir = -1;
    if (data->buttons & (maskDUp | maskDRight | maskDLeft | maskDDown)) {
      if (0==(data->buttons & (maskDRight| maskDLeft))) {
        if (data->buttons & maskDUp) 
          dir = 0;
        else 
          dir = 180;
      }
      else if (0==(data->buttons & (maskDUp | maskDDown))) {
        if (data->buttons & maskDRight)
          dir = 90;
        else
          dir = 270;
      }
      else if (data->buttons & maskDUp) {
        if (data->buttons & maskDRight)
          dir = 45;
        else if (data->buttons & maskDLeft)
          dir = 315;
      }
      else if (data->buttons & maskDDown) {
        if (data->buttons & maskDRight)
          dir = 135;
        else if (data->buttons & maskDLeft)
          dir = 225;
      }
    }
    Joystick.hat(dir);
}

void directionSwitchSlider(const GameControllerData_t* data, const EllipticalData_t* ellipticalP, int32_t multiplier) {
    if (ellipticalP->direction)
      Joystick.sliderRight(1023);
  }


uint16_t getEllipticalSpeed(const EllipticalData_t* ellipticalP, int32_t multiplier) {
  if (multiplier == 0) {
    if (ellipticalP->speed == 0)
      return 512;
    else if (ellipticalP->direction)
      return 1023;
    else
      return 0;
  }
  int32_t speed = 512+multiplier*(ellipticalP->direction?ellipticalP->speed:-ellipticalP->speed)/64;
  if (speed < 0)
    return 0;
  else if (speed > 1023)
    return 1023;
  else
    return speed;  
}

void joystickDualShoulder(const GameControllerData_t* data) {
    joystickBasic(data);
    joystickPOV(data);
    Joystick.sliderLeft(data->shoulderLeft);
    Joystick.sliderRight(data->shoulderRight);
}

void ellipticalSliders(const GameControllerData_t* data, const EllipticalData_t* ellipticalP, int32_t multiplier) {
#ifdef ENABLE_ELLIPTICAL
    if (debounceDown.getRawState() && data->device == CONTROLLER_NUNCHUCK) {
      // useful for calibration and settings for games: when downButton is pressed, joystickY controls both sliders
      if (data->joystickY >= 512+4*40 || data->joystickY <= 512-4*40) {
        int32_t delta = (512 - (int32_t)data->joystickY) * 49 / 40;
        uint16_t out;
        if (delta <= -511)
          out = 0;
        else if (delta >= 511)
          out = 1023;
        else
          out = 512 + delta;
        Joystick.sliderLeft(out);
        Joystick.sliderRight(out);
        debounceDown.cancelRelease();
        return;
       }
    }
    if(data->device == CONTROLLER_GAMECUBE && ! ellipticalP->valid)
      return;
    uint16_t datum = getEllipticalSpeed(ellipticalP, multiplier);
    Joystick.sliderLeft(datum);
    Joystick.sliderRight(datum);

#endif
}

void joystickUnifiedShoulder(const GameControllerData_t* data) {
    joystickBasic(data);
    joystickPOV(data);
    
    uint16_t datum;
    datum = (1024+data->shoulderRight-data->shoulderLeft)/2;
    Joystick.sliderLeft(datum);
    Joystick.sliderRight(datum);
}

void joystickNoShoulder(const GameControllerData_t* data) {
    joystickBasic(data);
    joystickPOV(data);
}

void inject(const Injector_t* injector, const GameControllerData_t* curDataP, const EllipticalData_t* ellipticalP) {
  didJoystick = false;

  if (prevInjector != injector) {
    Keyboard.releaseAll();
    Mouse.release(0xFF);
    for (int i=0; i<32; i++)
      Joystick.button(i, 0);
    prevInjector = injector;
  }

  memcpy(prevButtons, curButtons, sizeof(curButtons));
  toButtonArray(curButtons, curDataP);

  for (int i=0; i<numberOfButtons; i++) {
    if (injector->buttons[i].mode == KEY) {
      if (curButtons[i] != prevButtons[i]) {
        if (curButtons[i]) 
          Keyboard.press(injector->buttons[i].value.key);  
        else
          Keyboard.release(injector->buttons[i].value.key);
      }
    }
    else if (injector->buttons[i].mode == JOY) {
      Joystick.button(injector->buttons[i].value.button, curButtons[i]);
      didJoystick = true;
    }
    else if (injector->buttons[i].mode == MOUSE_RELATIVE) {
      if (!prevButtons[i] && curButtons[i])
        Mouse.move(injector->buttons[i].value.mouseRelative.x, injector->buttons[i].value.mouseRelative.y);
    }
    else if (injector->buttons[i].mode == CLICK) {
      if (!prevButtons[i] && curButtons[i])
        Mouse.click(injector->buttons[i].value.buttons);      
    }
  }

  if (injector->stick != NULL)
    injector->stick(curDataP);

  if (injector->elliptical != NULL)
    //ellipticalSliders(curDataP, ellipticalP);
    injector->elliptical(curDataP, ellipticalP, injector->ellipticalMultiplier);

  if (didJoystick)
    Joystick.sendManualReport();

}

#endif
