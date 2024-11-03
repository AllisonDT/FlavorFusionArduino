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
bool isSusanRotated = 0;
bool isRailForward = 0;
int numSpicesOrdered;         // Number of spices in current job

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
int encoderValue = 0;  // Count of encoder turns and cursor position
bool lastStateA = HIGH; // Previous state of encoder pin A


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

  // Setup picture loop
  u8g.firstPage();
  do {
    drawSetup();
  } while( u8g.nextPage());
}

void loop() {
  // Check if data is available from HM-10 (BLE central device)
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

  // Check if data is available from LCD
  /*
  updateLCD();
  */

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
  delay(100);
}

void sendOrderMixedStatus() {
  // Send the message to the Bluetooth app indicating the order is mixed
  Serial1.println("ORDER_MIXED:1");

  // quick beep for user
  analogWrite(LCD_beeper, 512);
  delay(1000);
  analogWrite(LCD_beeper, 0);

  // Finished job picture loop
  drawSummary();
  
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
    u8g.drawStr(w, 0, spiceArray[j][1].c_str()); // convert String object to const char*
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

  delay(2000); // Remove this line!!!
}

void drawSetup() {
  uint8_t h;
  
  u8g.setFont(u8g_font_5x8);
  u8g.setFontRefHeightText(); // calculate font height based on text only
  u8g.setFontPosTop(); // reference lines from top left instead of bottom left

  h = u8g.getFontAscent()-u8g.getFontDescent(); // font height

  u8g.drawStr( 0, 0, "HM-10 BLE Module Test");  
  u8g.drawStr( 0, h+1, "Waiting for data...");
}

void updateLCD() {
  // Read the current state of the encoder pins
  bool currentStateA = digitalRead(LCD_DT);
  bool currentStateB = digitalRead(LCD_CLK);

  // Determine if new input is received (LOW)
  if (lastStateA == HIGH && currentStateA == LOW) {
    // change cursor position based on direction of B
    encoderValue += (currentStateB == HIGH) ? 1 : -1;
    menu_redraw_required = 1;

    // Keep cursor in menu bounds
    if(encoderValue >= totalSpices){
      encoderValue = 0;
      delay(50); // slight delay to avoid jitter 
    }
    else if(encoderValue < 0){
      encoderValue = totalSpices-1;
      delay(50); // slight delay to avoid jitter 
    }
  }

  // Update last state
  lastStateA = currentStateA;
}

void drawSummary() {
  u8g.firstPage();
  do {
    u8g.drawStr(0, 0, "Complete! Stay Spicy!");
    int h = u8g.getFontAscent() - u8g.getFontDescent();
    u8g.drawStr(0, h+1, "Spice");
    u8g.drawStr(64, h+1, "Amount (oz)");
    u8g.drawHLine(0, 2*h+2, 128);

    for(int j = 0; j < numSpicesOrdered; j++) {
      u8g.drawStr(0, (j+2)*h+3, spiceArray[j][0].c_str()); // convert String object to const char*
      u8g.drawStr(64, (j+2)*h+3, spiceArray[j][1].c_str());
    }

    // eventually add a cursor to fit all spices on screen and allow user navigation

  } while( u8g.nextPage());
}