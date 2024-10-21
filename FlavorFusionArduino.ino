// Arduino Mega 2560 with HM-10 BLE Module
// HM-10 VCC to 3.3V (Note: HM-10 operates at 3.3V logic level; ensure voltage compatibility)
// HM-10 GND to GND
// HM-10 TXD to Arduino Mega RX1 (Pin 19)
// HM-10 RXD to Arduino Mega TX1 (Pin 18) (Use voltage divider to reduce 5V to 3.3V)

#include <algorithm>

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

// NEMA 11 for rail. Connected to X-step and X-dir
#define railStep A0 
#define railDir A1
#define railSleep 6

// NEMA 17 for carriage/ lazy susan. Connected to Y-step and Y-dir
#define susanStep A6
#define susanDir A7
#define susanSleep 5

// NEMA 8 for auger. Connected to Z-step and Z-dir
#define augerStep 46
#define augerDir 48
#define augerSleep 4

// Spice data arrays
String spiceArray[10][2];
int spiceIndex[10][2];


void setup() {
  // Set digital pins (and analog clones) to input/output
  pinMode(railStep, OUTPUT);
  pinMode(railDir, OUTPUT);
  pinMode(railSleep, OUTPUT);
  pinMode(susanStep, OUTPUT);
  pinMode(susanDir, OUTPUT);
  pinMode(susanSleep, OUTPUT);
  pinMode(augerStep, OUTPUT);
  pinMode(augerDir, OUTPUT);
  pinMode(augerSleep, OUTPUT);

  // Motor driver start-up
  digitalWrite(railSleep, HIGH);
  digitalWrite(susanSleep, HIGH);
  digitalWrite(augerSleep, HIGH);
  delay(100);

  // Motor driver sleep
  digitalWrite(railSleep, LOW);
  digitalWrite(susanSleep, LOW);
  digitalWrite(augerSleep, LOW);
  
  // Initialize Serial Monitor and Bluetooth (HM-10)
  Serial.begin(9600);
  Serial1.begin(9600);   // for HM-10 communication

  Serial.println("HM-10 Bluetooth Module Test");
  Serial.println("Waiting for incoming data from HM-10...");
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

        Serial.println("Complete data received from BLE:");
        Serial.println(incomingString);

        processReceivedIngredients(incomingString); // Process the complete command
        incomingString = ""; // Clear the buffer
      }
    }
  }

  // Process spice data if the order has been received and not mixed
  if (!isOrderMixed) {
    // Loop through all requested spices
    for (int j = 0; j < numSpicesOrdered; j++) {
      // Check if the spice amount is valid before proceeding
      if (spiceArray[j][1].toFloat() > 0) {
        Serial.println("\n========================================");

        // Move susan to the requested spice
        moveSusan(j);

        // Move rail forward
        moveRailForward();

        // Move auger for requested amount
        moveAuger(j);

        // Move rail back
        moveRailBackward();
      } else {
        Serial.println("Invalid spice amount, skipping this spice.");
      }
    }

    // Recalibrate after processing all spices
    calibrate();  
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
  Serial.println("Order mixed status sent: ORDER_MIXED:1");
}

// Custom comparator function for sorting spiceArray
bool compareSpices(const String a[2], const String b[2]) {
    return a[0].toInt() < b[0].toInt();
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

          Serial.print("Ingredient ID: ");
          Serial.print(ingredientID);
          Serial.print(", Amount: ");
          Serial.println(ingredientAmount);
        } else {
          Serial.println("Received invalid ingredient amount, skipping.");
        }
      }
    }
    start = separatorIndex + 1;
    separatorIndex = data.indexOf(';', start);
  }

  // Sort spice array by ingredient ID (container number)
  if (numSpicesOrdered > 0) {
      std::sort(spiceArray, spiceArray + numSpicesOrdered, compareSpices);
      isOrderMixed = false;
  } else {
      Serial.println("No valid ingredients found.");
  }
}

void moveSusan(int j) {
  Serial.print("Moving to container #");
  Serial.println(spiceArray[j][0]);

  // Exit sleep
  digitalWrite(susanSleep, HIGH);
  delay(5);

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

  Serial.println("Spice carriage motion complete\n");
  delay(1000);

  // Enter sleep
  digitalWrite(susanSleep, LOW);
}

void moveRailForward() {
  Serial.println("Moving rail forward");

  // Exit sleep
  digitalWrite(railSleep, HIGH);
  delay(5);

  // 20 revolutions for approx 0.75 in
  int numSteps = 20 * totalSteps;
  digitalWrite(railDir, LOW);

  // Eventually change this to be based on lim switch

  // Actuate
  for (int s = 0; s < numSteps; s++) {
    digitalWrite(railStep, HIGH); 
    delayMicroseconds(750);
    digitalWrite(railStep, LOW); 
    delayMicroseconds(750); 
  }

  Serial.println("Rail forward motion complete\n");
  delay(1000);

  // Enter sleep
  digitalWrite(railSleep, LOW);
}

void moveRailBackward() {
  Serial.println("Moving rail backward");

  // Exit sleep
  digitalWrite(railSleep, HIGH);
  delay(5);

  // 20 revolutions for approx 0.75 in
  int numSteps = 20 * totalSteps;
  digitalWrite(railDir, HIGH);

  for (int s = 0; s < numSteps; s++) {
    digitalWrite(railStep, HIGH); 
    delayMicroseconds(750);
    digitalWrite(railStep, LOW); 
    delayMicroseconds(750); 
  }

  Serial.println("Rail backward motion complete\n");
  delay(1000);

  // Enter sleep
  digitalWrite(railSleep, LOW);
}

void moveAuger(int j) {
  Serial.println("Moving auger");

  // Exit sleep
  digitalWrite(augerSleep, HIGH);
  delay(5);

  // calculate number of steps
  float spiceAmount = spiceArray[j][1].toFloat();
  int revPerOz = 20;
  int numSteps = spiceAmount * revPerOz * totalSteps;

  // Actuate
  digitalWrite(augerDir, LOW);
  for (int s = 0; s < numSteps; s++) {
    digitalWrite(augerStep, HIGH); 
    delayMicroseconds(5000);
    digitalWrite(augerStep, LOW); 
    delayMicroseconds(5000); 
  }

  Serial.print("Dispensed ");
  Serial.print(spiceAmount);
  Serial.println(" ounces of spice\n");
  delay(1000);

  // Enter sleep
  digitalWrite(augerSleep, LOW);
}

void calibrate() {
  isSusanRotated = 0;
}
