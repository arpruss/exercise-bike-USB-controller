#ifndef _DIGITALINPUT_H
#define _DIGITALINPUT_H

class DigitalInput {
  private:
    uint32 mask;
    volatile uint32* port;
  public:
    inline uint32 read() {
      return mask & *port;
    }
    
    DigitalInput(unsigned pin) {
      port = portInputRegister(digitalPinToPort(pin));
      mask = digitalPinToBitMask(pin);
    }
};

class DigitalOutput {
  private:
    uint32 setMask;
    uint32 resetMask;
    volatile uint32* bsrr;
  public:
    inline uint32 write(uint8_t value) {
      if (value)
        *bsrr = setMask;
      else
        *bsrr = resetMask;
    }
    
    DigitalInput(unsigned pin) {
      bsrr = &(digitalPinToPort(pin)->regs->BSRR);
      setMask = digitalPinToBitMask(pin);
      resetMask = setMask << 16;
    }
};

#endif
