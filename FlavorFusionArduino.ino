// Arduino Mega 2560 with HM-10 BLE Module
// HM-10 VCC to 3.3V (Note: HM-10 operates at 3.3V logic level; ensure voltage compatibility)
// HM-10 GND to GND
// HM-10 TXD to Arduino Mega RX1 (Pin 19)
// HM-10 RXD to Arduino Mega TX1 (Pin 18) (Use voltage divider to reduce 5V to 3.3V)

// Variables
String incomingString = "";
bool isOrderMixed = true;     // Track whether the whole order is mixed
bool isTrayEmpty = true;       // Track whether the tray is empty
String dataBuffer = "";        // Global buffer to accumulate incoming data
bool isSusanRotated = 0;
bool isRailForward = 0;

// Variables to track previous states
bool prevIsOrderMixed = false;
bool prevIsTrayEmpty = true;

// Motor control pins and setup
const int totalSteps = 200; //200 steps per revolution
const int totalMicrosteps = 3200; //1/16 microstepping: 3200 steps per revolution
const int totalSpices = 10;

// NEMA 11 for rail. Connected to E0-step and E0-dir
// #define railStep 26
// #define railDir 28
#define railStep A0 //try connecting to X-step and X-dir
#define railDir A1

// NEMA 17 for carriage/ lazy susan. Connected to Y-step and Y-dir
#define susanStep A6
#define susanDir A7

// NEMA 8 for auger. Connected to Z-step and Z-dir
#define augerStep 46
#define augerDir 48


// Spice data arrays
String spiceArray[10][2];
int spiceIndex[10][2];

// Number of spices ordered
int numSpicesOrdered;

void setup() {
  // Set analog pins to digital input/output
  pinMode(susanStep, OUTPUT);
  pinMode(susanDir, OUTPUT);
  pinMode(railStep, OUTPUT);
  pinMode(railDir, OUTPUT);
  
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

  // After processing all ingredients, mark the order as ready to be mixed
  if (numSpicesOrdered > 0) {
    isOrderMixed = false;
  } else {
    Serial.println("No valid ingredients found.");
  }
}

void moveSusan(int j) {
  Serial.println("Moving spice carriage");

  int prevSpice;
  if (j == 0) {
    prevSpice = 1; // Assume fully calibrated for first spice
  } else {
    prevSpice = spiceArray[j-1][0].toInt();
  }

  int spiceDiff = abs(spiceArray[j][0].toInt() - prevSpice);
  int numSteps = spiceDiff * totalSteps/totalSpices;

  digitalWrite(susanDir, LOW);
  for (int s = 0; s < numSteps; s++) {
    digitalWrite(susanStep, HIGH); 
    delayMicroseconds(5000);
    digitalWrite(susanStep, LOW); 
    delayMicroseconds(5000); 
  }

  Serial.println("Spice carriage motion complete");
  delay(1000);
}

void moveRailForward() {
  Serial.println("Moving rail forward");

  int numSteps = 20 * totalSteps;
  digitalWrite(railDir, LOW);

  for (int s = 0; s < numSteps; s++) {
    digitalWrite(railStep, HIGH); 
    delayMicroseconds(750);
    digitalWrite(railStep, LOW); 
    delayMicroseconds(750); 
  }

  Serial.println("Rail forward motion complete");
  delay(1000);
}

void moveRailBackward() {
  Serial.println("Moving rail backward");

  int numSteps = 20 * totalSteps;
  digitalWrite(railDir, HIGH);

  for (int s = 0; s < numSteps; s++) {
    digitalWrite(railStep, HIGH); 
    delayMicroseconds(750);
    digitalWrite(railStep, LOW); 
    delayMicroseconds(750); 
  }

  Serial.println("Rail backward motion complete");
  delay(1000);
}

void moveAuger(int j) {
  Serial.println("Moving auger");

  float spiceAmount = spiceArray[j][1].toFloat();
  int revPerOz = 20;
  int numSteps = spiceAmount * revPerOz * totalSteps;

  digitalWrite(augerDir, LOW);
  for (int s = 0; s < numSteps; s++) {
    digitalWrite(augerStep, HIGH); 
    delayMicroseconds(5000);
    digitalWrite(augerStep, LOW); 
    delayMicroseconds(5000); 
  }

  Serial.print("Dispensed ");
  Serial.print(spiceAmount);
  Serial.println(" ounces of spice");
  delay(1000);
}

void calibrate() {
  isSusanRotated = 0;
}
