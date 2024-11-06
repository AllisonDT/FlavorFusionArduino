// Arduino Mega 2560 with HM-10 BLE Module
// HM-10 VCC to 3.3V (Note: HM-10 operates at 3.3V logic level; ensure voltage compatibility)
// HM-10 GND to GND
// HM-10 TXD to Arduino Mega RX1 (Pin 19)
// HM-10 RXD to Arduino Mega TX1 (Pin 18) (Use voltage divider to reduce 5V to 3.3V)

#include "U8glib.h" // For LCD

// Variables
String incomingString = "";
bool isOrderMixed = true;     // Track whether the whole order is mixed
bool isTrayEmpty = true;       // Track whether the tray is empty
String dataBuffer = "";        // Global buffer to accumulate incoming data
bool isSusanRotated = 0; // Note: can prolly remove this since we'll calibrate every time
bool isRailForward = 0; // Note: can prolly remove this
int numSpicesOrdered;         // Number of spices in current job
bool isOrderInput = 0; // Note: can prolly remove this

// Variables to track previous states
bool prevIsOrderMixed = false;
bool prevIsTrayEmpty = true;

// Motor control pins and setup
const int totalSteps = 200; // 200 steps per revolution
const int totalMicrosteps = 3200; // 1/16 microstepping: 3200 steps per revolution
const int totalSpices = 10;

// NEMA 11 for rail. Connected to Y-step and Y-dir (needed for direction pin)
#define railStep A6
#define railDir A7
#define railEn A2

// NEMA 17 for carriage/ lazy susan. Connected to X-step and X-dir (don't need direction pin)
#define susanStep A0
#define susanDir A1
#define susanEn 38

// NEMA 8 for auger. Connected to Z-step and Z-dir
#define augerStep 46
#define augerDir 48
#define augerEn A8

// Spice data arrays
String spiceArray[10][2];
int spiceIndex[10][2];

// Define LCD pins
#define LCD_SCK 23 // SPI Com: SCK = en = 23 (LCD d4)
#define LCD_MOSI 17 // MOSI = rw = 17 (enable)
#define LCD_CS 16 // CS = di = 16 (rs)
#define LCD_DT 31 // rotary encoder pin 1, data (digital input)
#define LCD_CLK 33 // rotary encoder pin 2, clock (direction)
#define LCD_push 35 // rotary encoder push button
#define LCD_stop 41 // LCD stop button
#define LCD_beeper 37 // beeper on the LCD shield

// Define LCD Object
U8GLIB_ST7920_128X64_1X u8g(LCD_SCK, LCD_MOSI, LCD_CS);

// LCD variables
bool menu_redraw_required = 0;
int encoderValue = 0;  // Count of encoder turns = cursor position
bool lastStateA = HIGH; // Previous state of encoder pin A
uint8_t amountColumn = 0; // 0: spice integer,   1: spice fraction,   2: spice unit,   3: confirm
uint8_t maxBound = 1; // max index for a menu
uint8_t spiceListMax = 8; // maximum list capacity, to determine screen update
uint8_t summaryListMax = 5;
uint8_t currentMenu = 0; // 0: setup menu,   1: spice menu,   2: amount menu,   3: summary menu
unsigned long lastSummaryTime = 0; // time (millis) of last summary menu. For auto timeout.
unsigned long currentTime;
unsigned long interval = 15000; // 15 second interval before timeout
int pushState; // current button reading
int stopState;
int pushCount = 0;
int stopCount = 0;

// Buffer variables for spice amount menu:
int tempSpiceNumber = 0; 
int tempSpiceInt = 0;
float tempSpiceFraction = 0.0;
uint8_t tempSpiceDivider = 6; // unit conversion
String tempSpiceString = "";

const char *menu_strings[totalSpices+1] = { "Confirm order", "Spice 1", "Spice 2", "Spice 3", "Spice 4", "Spice 5", "Spice 6", "Spice 7", "Spice 8", "Spice 9", "Spice 10"};

const char *spicySaying[5] = {"Have a spicy day!",
        "You're on fire!",
        "Spice up your life!",
        "Blend it like it's hot!",
        "Keep it spicy!"};
long randomNumber; // for random spicy saying


void setup() {
  // Set digital pins (and analog clones) to input/output
  pinMode(railStep, OUTPUT);
  pinMode(railDir, OUTPUT);
  pinMode(railEn, OUTPUT);
  pinMode(susanStep, OUTPUT);
  pinMode(susanDir, OUTPUT);
  pinMode(susanEn, OUTPUT);
  pinMode(augerStep, OUTPUT);
  pinMode(augerDir, OUTPUT);
  pinMode(augerEn, OUTPUT);
  pinMode(LCD_DT, INPUT_PULLUP);
  pinMode(LCD_CLK, INPUT_PULLUP);
  pinMode(LCD_push, INPUT_PULLUP);
  pinMode(LCD_stop, INPUT_PULLUP);
  pinMode(LCD_beeper, OUTPUT);

  analogWrite(LCD_beeper, 0); // start beeper silent

  // Motor driver sleep
  digitalWrite(railEn, HIGH);
  digitalWrite(susanEn, HIGH);
  digitalWrite(augerEn, HIGH);
  
  // Initialize Bluetooth (HM-10)
  Serial1.begin(9600);

  u8g.setColorIndex(1);      // Set color to white  
  menu_redraw_required = 1;     // force initial redraw

  randomSeed(analogRead(A5)); // Note: ensure nothing is connected to A5 (in Aux-2 pins on Ramps)
}

void loop() {
  // Check if data is available from HM-10 (BLE central device)
  if(currentMenu == 0){
    if (Serial1.available()) {
      while (Serial1.available()) {
        char c = Serial1.read();
        incomingString += c;

        // Check if data contains the end marker "#END"
        if (incomingString.indexOf("#END") != -1) {
          // Remove the end marker and trim the data
          incomingString.replace("#END", "");
          incomingString.trim();

          processReceivedIngredients(incomingString); // Process the complete command
          incomingString = ""; // Clear the buffer
        }
      }
    }
  }

  // Update current LCD menu
  if(menu_redraw_required){
    u8g.firstPage();
    do{
      if(currentMenu == 0){
        // setup menu
        drawSetup();
      }else if(currentMenu == 1){
        // spice selection menu
        drawSpiceMenu();
      }else if(currentMenu == 2){
        // amount menu
        drawAmountMenu();
      }else{
        // summary menu
        drawSummary();
      }
    } while(u8g.nextPage());

    menu_redraw_required = 0;
  }

  // Check for user inputs
  updateLCD();

  // Reset to setup menu if summary has been up
  if(lastSummaryTime != 0){
    currentTime = millis();

    // If 15 seconds have passed:
    if(currentTime - lastSummaryTime >= interval){
      currentMenu = 0;
      menu_redraw_required = 1;
      lastSummaryTime = 0;
    }
  }

  // Process spice data if the order has been received and not mixed
  if (!isOrderMixed) {
    // calibrate carriage position
    calibrate();

    // Loop through all requested spices
    for (int j = 0; j < numSpicesOrdered; j++) {
      // Check if the spice amount is valid before proceeding
      if (spiceArray[j][1].toFloat() > 0) {
        // Move susan to the requested spice
        moveSusan(j);

        // Move rail forward
        moveRailForward();

        // Move auger for requested amount
        moveAuger(j);

        // Move rail back
        moveRailBackward();
      } else {

        u8g.firstPage();
        do{
          u8g.drawStr(0, 0, "Invalid spice amount");
          int h = u8g.getFontAscent() - u8g.getFontDescent();
          u8g.drawStr(0, h+1, "Skipping this spice");
        } while(u8g.nextPage());

      }
    }

    isOrderMixed = true; // Mark the order as complete

    // Send "ORDER_MIXED:1" to Bluetooth app to indicate the order is mixed
    sendOrderMixedStatus();
  }

  // Small delay to prevent overwhelming the BLE connection
  if(currentMenu == 0){
    delay(100);
  }
}

void sendOrderMixedStatus() {
  // Send the message to the Bluetooth app indicating the order is mixed
  Serial1.println("ORDER_MIXED:1");

  // quick beep for user
  analogWrite(LCD_beeper, 256); // comment out for quiet mode
  delay(1000);
  analogWrite(LCD_beeper, 0);

  // Set current menu to summary
  currentMenu = 3;  
  menu_redraw_required = 1;
  randomNumber = random(0, 5); // random spicy saying for the summary screen
}

void processReceivedIngredients(String data) {
  int start = 0;
  int separatorIndex = data.indexOf(';', start);
  numSpicesOrdered = 0;

  while (separatorIndex != -1) {
    String ingredientPair = data.substring(start, separatorIndex);
    if (ingredientPair.length() > 0) {
      int colonIndex = ingredientPair.indexOf(':');
      if (colonIndex != -1) {
        String ingredientID = ingredientPair.substring(0, colonIndex);
        String ingredientAmount = ingredientPair.substring(colonIndex + 1);

        // Ensure that the amount is properly parsed and valid
        float amount = ingredientAmount.toFloat();
        if (amount > 0) {
          spiceArray[numSpicesOrdered][0] = ingredientID;
          spiceArray[numSpicesOrdered][1] = ingredientAmount;
          numSpicesOrdered++;

        } else {
          u8g.firstPage();
          do{
            u8g.drawStr(0, 0, "Invalid spice amount");
            int h = u8g.getFontAscent() - u8g.getFontDescent();
            u8g.drawStr(0, h+1, "Skipping this spice");
          } while(u8g.nextPage());
        }
      }
    }
    start = separatorIndex + 1;
    separatorIndex = data.indexOf(';', start);
  }

  // Bubble sort spice array to ascending container number
  for (uint8_t c = 0; c < numSpicesOrdered - 1; c++) { // loop thru container numbers
    for (uint8_t d = 0; d < numSpicesOrdered - c - 1; d++) { // loop thru successive containers
      if (spiceArray[d][0].toInt() > spiceArray[d + 1][0].toInt()) { // compare
        for (uint8_t e = 0; e < 2; e++) {  // swap both container numbers and amounts
          String tempval = spiceArray[d][e];
          spiceArray[d][e] = spiceArray[d + 1][e]; 
          spiceArray[d + 1][e] = tempval;
        }
      }
    }
  }

  // After processing all ingredients, mark the order as ready to be mixed
  if (numSpicesOrdered > 0) {
    isOrderMixed = false;
  } else {
    u8g.firstPage();
        do{
          u8g.drawStr(0, 0, "No valid ingredients");
        } while(u8g.nextPage());
  }
}

void moveSusan(int j) {
  // Susan picture loop
  u8g.firstPage();
  do {
    u8g.drawStr(0, 0, "Moving to container #");
    int w = u8g.getStrPixelWidth("Moving to container #");
    u8g.drawStr(w+1, 0, spiceArray[j][0].c_str()); // convert String object to const char*
  } while( u8g.nextPage());

  // Exit sleep
  digitalWrite(susanEn, LOW);
  delay(50);

  // Find previous spice container
  int prevSpice;
  if (j == 0) {
    prevSpice = 1; // Assume fully calibrated for first spice
  } else {
    prevSpice = spiceArray[j-1][0].toInt();
  }

  // Calculate number of steps
  int spiceDiff = abs(spiceArray[j][0].toInt() - prevSpice);
  int numSteps = spiceDiff * totalSteps/totalSpices;

  // Actuate
  digitalWrite(susanDir, LOW);
  for (int s = 0; s < numSteps; s++) {
    digitalWrite(susanStep, HIGH); 
    delayMicroseconds(7500);
    digitalWrite(susanStep, LOW); 
    delayMicroseconds(7500); 
  }

  // Finished susan picture loop
  u8g.firstPage();
  do {
    u8g.drawStr(0, 0, "Carriage Motion Complete");
  } while( u8g.nextPage());

  delay(1000);

  // Enter sleep
  digitalWrite(susanEn, HIGH);
}

void moveRailForward() {
  // Rail fwd picture loop
  u8g.firstPage();
  do {
    u8g.drawStr(0, 0, "Moving rail forward");
  } while( u8g.nextPage());

  // Exit sleep
  digitalWrite(railEn, LOW);
  delay(50);

  // 20 revolutions for approx 0.75 in
  int numSteps = 10 * totalSteps;
  digitalWrite(railDir, LOW);
  delay(50);

  // Actuate
  for (int s = 0; s < numSteps; s++) {
    digitalWrite(railStep, HIGH); 
    delayMicroseconds(750);
    digitalWrite(railStep, LOW); 
    delayMicroseconds(750); 
  }

  // Finished rail picture loop
  u8g.firstPage();
  do {
    u8g.drawStr(0, 0, "Rail forward motion");
    int h = u8g.getFontAscent() - u8g.getFontDescent();
    u8g.drawStr(0, h+1, "complete");
  } while( u8g.nextPage());
  delay(1000);

  // Enter sleep
  digitalWrite(railEn, HIGH);
}

void moveRailBackward() {
  // Rail backward picture loop
  u8g.firstPage();
  do {
    u8g.drawStr(0, 0, "Moving rail backward");
  } while( u8g.nextPage());

  // Exit sleep
  digitalWrite(railEn, LOW);
  delay(50);

  // 20 revolutions for approx 0.75 in
  int numSteps = 10 * totalSteps;
  digitalWrite(railDir, HIGH);
  delay(50);

  for (int s = 0; s < numSteps; s++) {
    digitalWrite(railStep, HIGH); 
    delayMicroseconds(750);
    digitalWrite(railStep, LOW); 
    delayMicroseconds(750); 
  }

  // Finished rail picture loop
  u8g.firstPage();
  do {
    u8g.drawStr(0, 0, "Rail backward motion");
    int h = u8g.getFontAscent() - u8g.getFontDescent();
    u8g.drawStr(0, h+1, "complete");
  } while( u8g.nextPage());
  delay(1000);

  // Enter sleep
  digitalWrite(railEn, HIGH);
}

void moveAuger(int j) {
  // Auger picture loop
  u8g.firstPage();
  do {
    u8g.drawStr(0, 0, "Moving auger");
  } while( u8g.nextPage());

  // Exit sleep
  digitalWrite(augerEn, LOW);
  delay(50);

  // calculate number of steps
  float spiceAmount = spiceArray[j][1].toFloat();
  int revPerOz = 10;
  int numSteps = spiceAmount * revPerOz * totalSteps;

  // Actuate
  digitalWrite(augerDir, LOW);
  for (int s = 0; s < numSteps; s++) {
    digitalWrite(augerStep, HIGH); 
    delayMicroseconds(5000);
    digitalWrite(augerStep, LOW); 
    delayMicroseconds(5000); 
  }

  // Finished auger picture loop
  u8g.firstPage();
  do {
    u8g.drawStr(0, 0, "Dispensed ");
    int w = u8g.getStrPixelWidth("Dispensed ");

    // Find decimal point index
    int decimalIndex = spiceArray[j][1].indexOf('.');
    // Truncate string after 5 decimal places
    String truncatedString = spiceArray[j][1].substring(0, decimalIndex+6);
    // convert String object to const char*
    const char *truncatedAmount = truncatedString.c_str();

    u8g.drawStr(w, 0, truncatedAmount); 
    int h = u8g.getFontAscent() - u8g.getFontDescent();
    u8g.drawStr(0, h+1, "oz of spice");
  } while( u8g.nextPage());

  delay(1000);

  // Enter sleep
  digitalWrite(augerEn, HIGH);
}

void calibrate() {
  isSusanRotated = 0;

  // Calibration picture loop
  u8g.firstPage();
  do {
    u8g.drawStr(0, 0, "Calibrating...");
  } while( u8g.nextPage());  

  delay(2000); // Note: Remove this line!!!
}


void drawSetup() {
  uint8_t h;
  u8g_uint_t w, d;
  
  u8g.setFont(u8g_font_5x8);
  u8g.setFontRefHeightText(); // calculate font height based on text only
  u8g.setFontPosTop(); // reference lines from top left instead of bottom left

  h = u8g.getFontAscent()-u8g.getFontDescent(); // font height
  w = u8g.getWidth();

  d = (w - u8g.getStrPixelWidth("Waiting for order from"))/2;
  u8g.drawStr( d, 0, "Waiting for order from");  

  d = (w - u8g.getStrPixelWidth("Flavor Fusion app..."))/2;
  u8g.drawStr( d, h+1, "Flavor Fusion app...");

  d = (w - u8g.getStrPixelWidth("or"))/2;
  u8g.drawStr( d, 3*h+1, "or"); 

  d = (w - u8g.getStrPixelWidth("Click once to"))/2;
  u8g.drawStr( d, 5*h+1, "Click once to");
  d = (w - u8g.getStrPixelWidth("order manually."))/2;
  u8g.drawStr( d, 6*h+1, "order manually.");
}


void updateLCD() {
  // Read the current state of the encoder pins
  // bool currentStateA = digitalRead(LCD_DT);
  // bool currentStateB = digitalRead(LCD_CLK);

  static uint8_t state = 0;
  bool CLKstate = digitalRead(LCD_CLK);
  bool DTstate = digitalRead(LCD_DT);
  switch (state) {
      case 0:                         // Idle state, encoder not turning
          if (!CLKstate){             // Turn CCW and CLK goes low first
              state = 1;
          } else if (!DTstate) {      // Turn CW and DT goes low first
              state = 4;
          }
          break;
      // CCW rotation
      case 1:                     
          if (!DTstate) {             // Continue CCW and DT will go low after CLK
              state = 2;
          } 
          break;
      case 2:
          if (CLKstate) {             // Turn further and CLK will go high first
              state = 3;
          }
          break;
      case 3:
          if (CLKstate && DTstate) {  // Both CLK and DT now high as the encoder completes one step CCW
              state = 0;
              encoderValue--;
              menu_redraw_required = 1;
          }
          break;
      // CW rotation
      case 4:                         // As for CCW but with CLK and DT reversed
          if (!CLKstate) {
              state = 5;
          }
          break;
      case 5:
          if (DTstate) {
              state = 6;
          }
          break;
      case 6:
          if (CLKstate && DTstate) {
              state = 0;
              encoderValue++;
              menu_redraw_required = 1;
          }
          break; 
  }

  // // Determine if new input is received (LOW)
  // if (lastStateA == HIGH && currentStateA == LOW) {
  //   // change cursor position based on direction of B
  //   encoderValue += (currentStateB == HIGH) ? 1 : -1;
  //   //delay(100); // slight delay for smoothness. Consider changing this to debounce delay
    
  if(menu_redraw_required){
    // Keep cursor in menu bounds
    if(currentMenu==1){
      // Spice menu bounds
      maxBound = totalSpices; // 10 spices plus confirm button
    }else if(currentMenu==2){
      // Amount menu bounds
      if(amountColumn == 0){
        // Spice amount (integer)
        maxBound = 9; // limit selection to 9

      }else if(amountColumn == 1){
        // Spice amount (fraction)
        maxBound = 7; // limit selection to 7/8

      }else if(amountColumn == 2){
        // Spice unit
        maxBound = 2; // tsp, Tbsp, oz
      }
    }else if(currentMenu==3){
      // Summary menu bounds
      maxBound = numSpicesOrdered-1;
    }

    if(encoderValue > maxBound){
        encoderValue = 0;
        delay(50); // slight delay to avoid jitter 
      }else if(encoderValue < 0){
        encoderValue = maxBound;
        delay(50); // slight delay to avoid jitter 
      }
  }

  // Update last state
  // lastStateA = currentStateA;

  // Read encoder button
  pushState = digitalRead(LCD_push);
  // If button is not pressed, reset counter
  if(pushState == HIGH){
    pushCount = 0;
  }

  // if button pressed and not previously counted
  if(pushState == LOW && pushCount == 0){
    menu_redraw_required = 1;
    
    // Change menu or column depending on curent screen
    if(currentMenu == 1){
      // Spice menu:
      if(encoderValue == 0){
        processReceivedIngredients(tempSpiceString); // process data into spiceArray
        tempSpiceString = ""; // clear buffer
        pushCount = 1; // prevent button bouncing
      }else{
      tempSpiceNumber = encoderValue; // save selected spice into buffer
      currentMenu++; // advance from spice menu to amount menu
      encoderValue = 0; // reset cursor value
      pushCount = 1; // prevent button bouncing
    }
    // Amount menu:
    }else if(currentMenu == 2 && amountColumn != 3){
      // save selected amount into buffer
      if(amountColumn == 0){
        tempSpiceInt = encoderValue; // whole number
      }else if(amountColumn == 1){
        tempSpiceFraction = encoderValue*0.125; // fractions (1/8 increment)
      }else{
        if(encoderValue == 0){
          tempSpiceDivider = 6; // convert tsp to oz
        }else if(encoderValue == 1){
          tempSpiceDivider = 2; // convert Tbsp to oz
        }else{
          tempSpiceDivider = 1; // convert oz to oz
        }
      }
      amountColumn++; // advance column number in amount menu
      encoderValue = 0; // reset cursor
      pushCount = 1; // prevent button bouncing
    }else if(currentMenu == 2 && amountColumn == 3){
      tempSpiceString += String(tempSpiceNumber) + ":"; // store spice number and amount 
      tempSpiceString += String((tempSpiceInt + tempSpiceFraction) / tempSpiceDivider) + ";";
      tempSpiceNumber = 0; // clear buffer
      tempSpiceInt = 0;
      tempSpiceFraction = 0.0;
      tempSpiceDivider = 6;
      amountColumn = 0; // reset column
      currentMenu = 1; // if amount confirmed, return to spice menu
      pushCount = 1; // prevent button bouncing
    }else if(currentMenu == 3){
      // Summary screen
      currentMenu = 0; // return to setup menu
      pushCount = 1; // prevent button bouncing
    }else{
      // Setup screen
      currentMenu = 1; // advance to spice menu
      pushCount = 1; // prevent button bouncing
    }
  }

  // Read stop button
  stopState = digitalRead(LCD_stop);
  // If button is not pressed, reset counter
  if(stopState == HIGH){
    stopCount = 0;
  }

  // if button pressed and not previously counted
  if(stopState == LOW && stopCount == 0){
    menu_redraw_required = 1;

    // Change menu or column depending on curent screen
    if(currentMenu == 1){
      // Spice menu:
      currentMenu--; // return to setup menu
      tempSpiceString = ""; // wipe data buffer
      encoderValue = 0; // reset cursor
      stopCount = 1;
    }else if(currentMenu == 2 && amountColumn != 0){
      // Amount menu:
      amountColumn--; // retract column number in amount menu
      encoderValue = 0; // reset cursor
      stopCount = 1;
    }else if(currentMenu == 2 && amountColumn == 0){
      currentMenu--; // return to spice menu
      encoderValue = 0; // reset cursor
      stopCount = 1;
    }else if(currentMenu == 3){
      // Summary screen
      currentMenu = 0; // reset to setup screen
      stopCount = 1;
    }
  }  
}

void drawSummary() {
  u8g.firstPage();
  do {
    uint8_t j, h;
    u8g_uint_t w, d;
    
    w = u8g.getWidth();
    d = (w-u8g.getStrWidth("Blending complete!"))/2;
    u8g.drawStr(d, 0, "Blending complete!");
    h = u8g.getFontAscent() - u8g.getFontDescent();

    d = (w-u8g.getStrWidth(spicySaying[randomNumber]))/2;
    u8g.drawStr(d, h+1, spicySaying[randomNumber]);

    u8g.drawStr(0, 2*h+1, "Spice");
    u8g.drawStr(64, 2*h+1, "Amount (oz)");
    u8g.drawHLine(0, 3*h+2, 128);

    for(j = 0; j < numSpicesOrdered; j++) {
      u8g.drawStr(0, (j+3)*h+3, spiceArray[j][0].c_str()); // convert String object to const char*

      // Find decimal point index
      int decimalIndex = spiceArray[j][1].indexOf('.');
      // Truncate string after 5 decimal places
      String truncatedString = spiceArray[j][1].substring(0, decimalIndex+6);
      // convert String object to const char*
      const char *truncatedAmount = truncatedString.c_str();

      u8g.drawStr(64, (j+3)*h+3, truncatedAmount);
    }

    // eventually add a cursor to fit all spices on screen and allow user navigation

  } while( u8g.nextPage());

  lastSummaryTime = millis();
}

void drawSpiceMenu() {
  uint8_t i, h;
  u8g_uint_t w, d;

  u8g.setFont(u8g_font_5x8);
  u8g.setFontRefHeightText(); // line height based on text chars
  u8g.setFontPosTop(); // line position references top left of text
  
  h = u8g.getFontAscent()-u8g.getFontDescent(); // text height
  w = u8g.getWidth(); // screen width
  for( i = 0; i < totalSpices; i++ ) {
    d = (w-u8g.getStrWidth(menu_strings[i]))/2;
    u8g.setDefaultForegroundColor();
    if ( i == encoderValue ) {
      u8g.drawBox(0, i*h+1, w, h);
      u8g.setDefaultBackgroundColor();
    }
    u8g.drawStr(d, i*h, menu_strings[i]);

    // ADD LIST CASCADE 

  }
}

void drawAmountMenu(){
  uint8_t i, h;
  u8g_uint_t w, d, l, s, m;

  u8g.setFont(u8g_font_5x8);
  u8g.setFontRefHeightText(); // line height based on text chars
  u8g.setFontPosTop(); // line position references top left of text

  h = u8g.getFontAscent()-u8g.getFontDescent(); // text height
  w = u8g.getWidth(); // screen width

  // Spice name at top of menu
  char buffer[10];
  itoa(tempSpiceNumber, buffer, 10);
  const char *spiceNumber = buffer;
  d = (w - (u8g.getStrWidth("Spice ") + u8g.getStrWidth(spiceNumber)))/2;
  u8g.drawStr(d, 0, "Spice ");
  u8g.drawStr(d+u8g.getStrWidth("Spice "), 0, spiceNumber);

  // define labels
  const char *labels[4] = {"Whole", "Fraction", "Unit", "Done"};
  s = (w - u8g.getStrWidth("WholeFractionUnitDone")) / 3; // space between labels
  l = 0; //length of labels so far
  
  // Loop thru columns
  for(i = 0; i < 4; i++){

    u8g.drawStr(l, 2*h, labels[i]); // labels

    m = l + (u8g.getStrWidth(labels[i]))/2; // triangle midpoint

    // check mark
    uint8_t checkWidth = 24;
    uint8_t checkHeight = 16;
    const uint8_t checkmark_bits[] = {
    0x00, 0x00, 0x00,  // Row 0
    0x00, 0x00, 0x70,  // Row 1
    0x00, 0x00, 0xE0,  // Row 2
    0x00, 0x01, 0xC0,  // Row 3
    0x00, 0x03, 0x80,  // Row 4
    0x00, 0x07, 0x00,  // Row 5
    0x00, 0x0E, 0x00,  // Row 6
    0x00, 0x1C, 0x00,  // Row 7
    0x00, 0x38, 0x00,  // Row 8
    0x60, 0x70, 0x00,  // Row 9
    0x70, 0xE0, 0x00,  // Row 10
    0x31, 0xC0, 0x00,  // Row 11
    0x3B, 0x80, 0x00,  // Row 12
    0x1F, 0x00, 0x00,  // Row 13
    0x0E, 0x00, 0x00,  // Row 14
    0x04, 0x00, 0x00   // Row 15
    };
    
    // check for cursor box
    if (i == amountColumn) {
      if(i != 3){
        // Selection values
        if(i==0){
          u8g.drawBox(m-5, 3*h+2, 11, 18); //cursor box
          u8g.setDefaultBackgroundColor();
          u8g.drawTriangle(m-4,4*h, m+4,4*h, m,3*h+2); // top triangle
          u8g.drawTriangle(m-4,5*h+1, m,6*h-2, m+4,5*h+1); // bottom triangle
          itoa(encoderValue, buffer, 10);
          u8g.drawStr(m-2, 4*h, buffer); // whole number
        }else if(i==1){
          u8g.setColorIndex(1);
          u8g.drawBox(m-13, 3*h+2, 27, 18); // larger box
          u8g.setDefaultBackgroundColor();
          u8g.drawTriangle(m-4,4*h, m+4,4*h, m,3*h+2); // top triangle
          u8g.drawTriangle(m-4,5*h+1, m,6*h-2, m+4,5*h+1); // bottom triangle
          if(encoderValue == 1){
            u8g.drawStr(m-7, 4*h, "1/8"); // fraction
          }else if(encoderValue == 2){
            u8g.drawStr(m-7, 4*h, "1/4"); 
          }else if(encoderValue == 3){
            u8g.drawStr(m-7, 4*h, "3/8"); 
          }else if(encoderValue == 4){
            u8g.drawStr(m-7, 4*h, "1/2"); 
          }else if(encoderValue == 5){
            u8g.drawStr(m-7, 4*h, "5/8"); 
          }else if(encoderValue == 6){
            u8g.drawStr(m-7, 4*h, "3/4"); 
          }else if(encoderValue == 7){
            u8g.drawStr(m-7, 4*h, "7/8"); 
          }else{
            u8g.drawStr(m-2, 4*h, "0");
          }
          
        }else if(i==2){
          u8g.drawBox(m-11, 3*h+2, 23, 18); //medium sized cursor box
          u8g.setDefaultBackgroundColor();
          u8g.drawTriangle(m-4,4*h, m+4,4*h, m,3*h+2); // top triangle
          u8g.drawTriangle(m-4,5*h+1, m,6*h-2, m+4,5*h+1); // bottom triangle

          if(encoderValue==2){
            u8g.drawStr(m-u8g.getStrWidth("oz")/2, 4*h, "oz"); // unit: oz
          }else if(encoderValue==1){
            u8g.drawStr(m-u8g.getStrWidth("Tbsp")/2, 4*h, "Tbsp"); // unit: Tbsp
          }else if(encoderValue==0){
            u8g.drawStr(m-u8g.getStrWidth("tsp")/2, 4*h, "tsp"); // unit: tsp
          }
        }
      }else{
        u8g.drawBox(m-11, 3*h+2, 24, 18);
        u8g.setDefaultBackgroundColor();
        u8g.drawBitmap(m-10, 3*h+3, checkWidth/8, checkHeight, checkmark_bits); //draw checkmark
      }
      u8g.setColorIndex(1);
    }else{
      // else print normally

      // Triangles
      if(i != 3){
      u8g.drawTriangle(m-4,4*h, m+4,4*h, m,3*h+2); // top triangle
      u8g.drawTriangle(m-4,5*h+1, m,6*h-2, m+4,5*h+1); // bottom triangle
      }

      // Selection values
      if(i==0){
        itoa(tempSpiceInt, buffer, 10);
        u8g.drawStr(m-2, 4*h, buffer); // whole number
      }else if(i==1){
        if(tempSpiceFraction == 0.125){
            u8g.drawStr(m-7, 4*h, "1/8"); // fraction
          }else if(tempSpiceFraction == 0.25){
            u8g.drawStr(m-7, 4*h, "1/4"); 
          }else if(tempSpiceFraction == 0.375){
            u8g.drawStr(m-7, 4*h, "3/8"); 
          }else if(tempSpiceFraction == 0.5){
            u8g.drawStr(m-7, 4*h, "1/2"); 
          }else if(tempSpiceFraction == 0.625){
            u8g.drawStr(m-7, 4*h, "5/8"); 
          }else if(tempSpiceFraction == 0.75){
            u8g.drawStr(m-7, 4*h, "3/4"); 
          }else if(tempSpiceFraction == 0.875){
            u8g.drawStr(m-7, 4*h, "7/8"); 
          }else{
            u8g.drawStr(m-2, 4*h, "0");
          }
      }else if(i==2){
        if(tempSpiceDivider==1){
          u8g.drawStr(m-u8g.getStrWidth("oz")/2, 4*h, "oz"); // unit: oz
        }else if(tempSpiceDivider==2){
          u8g.drawStr(m-u8g.getStrWidth("Tbsp")/2, 4*h, "Tbsp"); // unit: Tbsp
        }else if(tempSpiceDivider==6){
          u8g.drawStr(m-u8g.getStrWidth("tsp")/2, 4*h, "tsp"); // unit: tsp
        }
      }

      // Check mark
      if(amountColumn != 3 && i == 3){
      u8g.drawBitmap(m-10, 3*h+3, checkWidth/8, checkHeight, checkmark_bits);
    }
      
    }

    l += u8g.getStrWidth(labels[i]) + s;
  }

}
