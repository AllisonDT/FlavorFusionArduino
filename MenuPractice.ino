#include "U8glib.h"

// *** define LCD pins***
#define LCD_SCK 23 // SPI Com: SCK = en = 23 (LCD d4)
#define LCD_MOSI 17 // MOSI = rw = 17 (enable)
#define LCD_CS 16 // CS = di = 16 (rs)
#define LCD_DT 31 // rotary encoder pin 1, data (digital input)
#define LCD_CLK 33 // rotary encoder pin 2, clock (direction)
#define LCD_push 35 // rotary encoder push button
#define LCD_stop 41 // LCD stop button
#define LCD_beeper 37 // beeper on the LCD shield

U8GLIB_ST7920_128X64_1X u8g(LCD_SCK, LCD_MOSI, LCD_CS);

#define MENU_ITEMS 5
const char *menu_strings[MENU_ITEMS] = { "Spice 1", "Spice 2", "Spice 3", "Spice 4", "Spice 5"};

bool menu_redraw_required = 0;
int encoderValue = 0;  // Count of encoder turns and cursor position
bool lastStateA = HIGH; // Previous state of encoder pin A


void setup() {

  pinMode(LCD_DT, INPUT_PULLUP);
  pinMode(LCD_CLK, INPUT_PULLUP);
  pinMode(LCD_push, INPUT_PULLUP);
  pinMode(LCD_stop, INPUT_PULLUP);
  pinMode(LCD_beeper, OUTPUT);

  analogWrite(LCD_beeper, 0); // beeper quiet upon start

  menu_redraw_required = 1;     // force initial redraw
}

void loop() {  

  updateEncoder();      // check for rotary encoder input
    
  if (menu_redraw_required) {
    u8g.firstPage();
    do  {
      drawMenu();
    } while( u8g.nextPage() );
    menu_redraw_required = 0;
  }
  
}

void updateEncoder() {
  // Read the current state of the encoder pins
  bool currentStateA = digitalRead(LCD_DT);
  bool currentStateB = digitalRead(LCD_CLK);

  // Determine if new input is received (LOW)
  if (lastStateA == HIGH && currentStateA == LOW) {
    // change cursor position based on direction of B
    encoderValue += (currentStateB == HIGH) ? 1 : -1;
    menu_redraw_required = 1;

    // Keep cursor in menu bounds
    if(encoderValue >= MENU_ITEMS){
      encoderValue = 0;
      delay(50); // slight delay to avoid jitter 
    }
    else if(encoderValue < 0){
      encoderValue = MENU_ITEMS-1;
      delay(50); // slight delay to avoid jitter 
    }
  }

  // Update last state
  lastStateA = currentStateA;
}

void drawMenu(void) {
  uint8_t i, h;
  u8g_uint_t w, d;

  u8g.setFont(u8g_font_5x8);
  u8g.setFontRefHeightText();
  u8g.setFontPosTop();
  
  h = u8g.getFontAscent()-u8g.getFontDescent();
  w = u8g.getWidth();
  for( i = 0; i < MENU_ITEMS; i++ ) {
    d = (w-u8g.getStrWidth(menu_strings[i]))/2;
    u8g.setDefaultForegroundColor();
    if ( i == encoderValue ) {
      u8g.drawBox(0, i*h+1, w, h);
      u8g.setDefaultBackgroundColor();
    }
    u8g.drawStr(d, i*h, menu_strings[i]);
  }
}
