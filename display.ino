#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h> // use fork at github.com/arpruss
#include "6pt.h"

#define SCREEN_WIDTH 84
#define SCREEN_HEIGHT 48
#define TEXT_HEIGHT 6
#define NUM_LINES (SCREEN_HEIGHT/TEXT_HEIGHT)

static uint8_t* pcd8544_buffer;
static SPIClass mySPI(1);
static Adafruit_PCD8544 display = Adafruit_PCD8544(PIN_SCREEN_DC, 0/*CS*/, PIN_SCREEN_RST, &mySPI); // warning: MISO is input, NSS is output
static int displayedInjector = -1;
static int topInjector;
static int displayedController = -1;
static int displayedRPM = -1;
static int maxControllerLength;
static int maxRPMLength;
static int cursorLength=4;
static const int numInjectorLines = NUM_LINES-1;

unsigned measureText(const char* text) {
  unsigned w = 0;
  while(*text) {
    uint8_t c = *text++;
    if (c<32 || c>126)
      c=0;
    else
      c-=32;
    w += font_widths[c];
  }
  return w;
}

void drawCursor(int line, uint8 color) {
  int y0 = line * TEXT_HEIGHT + 2;
  display.drawPixel(2,y0,color);
  display.drawPixel(1,y0-1,color);
  display.drawPixel(1,y0,color);
  display.drawPixel(1,y0+1,color);
  display.drawPixel(0,y0,color);
  display.drawPixel(0,y0-2,color);
  display.drawPixel(0,y0-1,color);
  display.drawPixel(0,y0+1,color);
  display.drawPixel(0,y0+2,color);
}

void displayInit() {
  mySPI.begin();
  mySPI.setModule(1);
  pcd8544_buffer = display.getPixelBuffer();
  maxControllerLength = 0;
  for (int i=0; i<numControllers; i++) {
    int l = measureText(controllerNames[i]);
    if (l > maxControllerLength)
      maxControllerLength = l;
  }    
  maxRPMLength = measureText("-999");
  display.begin(); //40,5
  display.clearDisplay();
}

template<bool shiftLeft, bool invert>void writePartialLine(const char* text, unsigned xStart, unsigned xEnd, unsigned line, unsigned shift) {
  uint8_t* out = pcd8544_buffer+line*SCREEN_WIDTH + xStart;
  uint8_t* end = out + xEnd-xStart;
  uint8_t mask = shiftLeft ? ((1<<TEXT_HEIGHT)-1)<<shift : ((1<<TEXT_HEIGHT)-1)>>shift;
  if (!invert)
    mask = ~mask;
  while (out < end && *text) {
    uint8_t c = *text++;
    if (c < 32 || c > 126) 
      c = 0;
    else
      c -= 32;
    uint8_t width = font_widths[c];
    const uint8_t* f = font_data[c];
    do {
      if (invert) {
        if (shiftLeft) 
          *out = (*out | mask) ^ (*f << shift);
        else
          *out = (*out | mask) ^ (*f >> shift);
      }
      else {
        if (shiftLeft) 
          *out = (*out & mask) | (*f << shift);
        else
          *out = (*out & mask) | (*f >> shift);
      }
      width--;
      f++;
      out++;
    }
    while (width && out < end);
  }
  while (out < end) {
    if (invert) {
      *out |= mask;
    }
    else {
      *out &= mask;
    }
    out++;
  }  
}

void writeText(const char* text, unsigned xStart, unsigned xEnd, unsigned line, bool invert=false, bool rightAlign=false) {
  int y = line * TEXT_HEIGHT;
  if (xStart >= SCREEN_WIDTH || y >= SCREEN_HEIGHT || xStart >= xEnd)
    return;
  if (xEnd >= SCREEN_WIDTH)
    xEnd = SCREEN_WIDTH;
  unsigned line1 = y>>3;
  unsigned modulus = y&0x7;
  
  if (rightAlign) {
    unsigned w = measureText(text);
    if (w + xStart + 1 < xEnd) {
      writeText("", xStart, xEnd-w, y, invert, false);
      xStart = xEnd-w;
    }
  }
  
  if (invert)
    writePartialLine<true,true>(text, xStart, xEnd, line1, modulus);
  else
    writePartialLine<true,false>(text, xStart, xEnd, line1, modulus);
  line1++;
  if (modulus>8-TEXT_HEIGHT && line1 < (SCREEN_HEIGHT>>3)) {
    if(invert)
      writePartialLine<false,true>(text, xStart, xEnd, line1, 8-modulus);
    else
      writePartialLine<false,false>(text, xStart, xEnd, line1, 8-modulus);
  }
  display.updateBoundingBox(xStart, y, xEnd, y+TEXT_HEIGHT-1);
}

void displayInjector(int i) {
  if (i==displayedInjector)
    return;
  if (displayedInjector >= 0 && topInjector <= i && i < topInjector + numInjectorLines) {
    drawCursor(displayedInjector-topInjector+1, 0);
    drawCursor(i-topInjector+1, 1);
//    writeText(injectors[displayedInjector].displayName, cursorLength, SCREEN_WIDTH, displayedInjector - topInjector + 1);
//    writeText(injectors[i].displayName, 0, SCREEN_WIDTH, i - topInjector + 1, true);
  }
  else {
    if (displayedInjector < 0) {
      topInjector = i - numInjectorLines + 1;
      if (topInjector < 0)
          topInjector = 0;
    }
    else if (i < topInjector) {
      topInjector = i;
    }    
    else if (i >= topInjector+numInjectorLines) {
      topInjector = i - numInjectorLines + 1;
    }
    for (int j=0; j<numInjectorLines; j++) {
      drawCursor(1+j, topInjector+j == i);
      writeText(injectors[topInjector+j].displayName, cursorLength, SCREEN_WIDTH, 1+j); //, topInjector+j == i);
    }
  }
  displayedInjector = i;
  display.display();
}

void displayController(int i) {
  if (i<0 || i>sizeof(controllerNames)/sizeof(*controllerNames))
    i = 0;
  if (i == displayedController)
    return;
  writeText(controllerNames[i], SCREEN_WIDTH - maxRPMLength-maxControllerLength, SCREEN_WIDTH-maxRPMLength, 0, false); 
  displayedController = i;
  display.display();
}

void displayRPM(int rpm) {
  if (rpm == displayedRPM)
    return;
  if (rpm < 0) {
    writeText("", SCREEN_WIDTH - maxRPMLength, SCREEN_WIDTH, 0, false, false);
  }
  else {
    char rpmText[11];
    sprintf(rpmText, "%d", rpm);
    writeText(rpmText, SCREEN_WIDTH - maxRPMLength, SCREEN_WIDTH, 0, false, true);
  }
  displayedRPM = rpm; 
  display.display();
}

