// Arduino Mega 2560 with HM-10 BLE Module
// HM-10 VCC to 3.3V (Note: HM-10 operates at 3.3V logic level; ensure voltage compatibility)
// HM-10 GND to GND
// HM-10 TXD to Arduino Mega RX1 (Pin 19)
// HM-10 RXD to Arduino Mega TX1 (Pin 18) (Use voltage divider to reduce 5V to 3.3V)

// Variables
String incomingString = "";
bool isOrderMixed = false;     // Track whether the whole order is mixed
bool isTrayEmpty = true;       // Track whether the tray is empty
String dataBuffer = "";        // Global buffer to accumulate incoming data

// Variables to track previous states
bool prevIsOrderMixed = false;
bool prevIsTrayEmpty = true;

void setup() {
  Serial.begin(9600);    // Initialize Serial Monitor
  Serial1.begin(9600);   // Initialize Serial1 for HM-10 communication

  Serial.println("HM-10 Bluetooth Module Test");
  Serial.println("Waiting for incoming data from HM-10...");
}

void loop() {
  // Check if data is available from HM-10 (BLE central device)
  if (Serial1.available()) {
    while (Serial1.available()) {
      char c = Serial1.read();
      Serial.print(c);  // For debugging: print received character to Serial Monitor
      incomingString += c;

      // Check if data contains the end marker "#END"
      if (incomingString.indexOf("#END") != -1) {
        // Remove the end marker from the buffer
        incomingString.replace("#END", "");
        incomingString.trim(); // Remove any leading/trailing whitespace

        Serial.println("\nComplete data received from BLE:");
        Serial.println(incomingString);

        processBLECommand(incomingString); // Process the complete command
        incomingString = ""; // Clear the buffer
      }
    }
  }

  // Check for serial input from Serial Monitor
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();                          // Trim the string in place
    processSerialInput(input);             // Pass the trimmed string
  }

  // Check for status changes and send updates if necessary
  checkAndSendStatusUpdates();

  // Small delay to prevent overwhelming the BLE connection
  delay(100);
}

void processBLECommand(String command) {
  Serial.print("Processing BLE command: ");
  Serial.println(command);

  // Since we are directly receiving the serialized ingredients, process them
  processReceivedIngredients(command);
}

void processReceivedIngredients(String data) {
  Serial.println("Processing received ingredients data:");

  int start = 0;
  int separatorIndex = data.indexOf(';', start);

  while (separatorIndex != -1) {
    String ingredientPair = data.substring(start, separatorIndex);
    if (ingredientPair.length() > 0) {  // Skip empty pairs
      int colonIndex = ingredientPair.indexOf(':');
      if (colonIndex != -1) {
        String ingredientID = ingredientPair.substring(0, colonIndex);
        String ingredientAmount = ingredientPair.substring(colonIndex + 1);

        Serial.print("Ingredient ID: ");
        Serial.print(ingredientID);
        Serial.print(", Amount: ");
        Serial.println(ingredientAmount);

        // TODO: Add code to actuate hardware based on ingredientID and ingredientAmount

      } else {
        Serial.println("Invalid ingredient format.");
      }
    }

    start = separatorIndex + 1;
    separatorIndex = data.indexOf(';', start);
  }

  // After processing all ingredients, mark the order as mixed
  markOrderAsMixed();
}

void markOrderAsMixed() {
  isOrderMixed = true;  // Once all ingredients are processed, mark the order as mixed
  Serial.println("Order has been marked as mixed.");
}

void resetOrderMixed() {
  isOrderMixed = false;  // Reset the order mixed status
  Serial.println("Order has been reset and marked as not mixed.");
}

void processSerialInput(String input) {
  // Handle input from Serial Monitor for testing
  if (input.equalsIgnoreCase("complete")) {
    markOrderAsMixed();  // Mark the blend as completed
    Serial.println("Manually marked the order as mixed.");
  } else if (input.equalsIgnoreCase("reset")) {
    resetOrderMixed();  // Unmark the blend as completed
    Serial.println("Manually reset the order as not mixed.");
  } else if (input.equalsIgnoreCase("tray empty")) {
    isTrayEmpty = true;
    Serial.println("Manually set tray as empty.");
  } else if (input.equalsIgnoreCase("tray not empty")) {
    isTrayEmpty = false;
    Serial.println("Manually set tray as not empty.");
  } else {
    Serial.println("Invalid input. Use 'complete'/'reset' or 'tray empty'/'tray not empty' to simulate actions.");
  }
}

void sendOrderMixedStatus() {
  String status = isOrderMixed ? "ORDER_MIXED:1" : "ORDER_MIXED:0";
  Serial1.println(status);
  Serial.print("Sent to BLE: ");
  Serial.println(status);
}

void sendTrayEmptyStatus() {
  String status = isTrayEmpty ? "TRAY_EMPTY:1" : "TRAY_EMPTY:0";
  Serial1.println(status);
  Serial.print("Sent to BLE: ");
  Serial.println(status);
}

void checkAndSendStatusUpdates() {
  // Check if the order mixed status has changed
  if (isOrderMixed != prevIsOrderMixed) {
    sendOrderMixedStatus();
    prevIsOrderMixed = isOrderMixed;
  }

  // Check if the tray empty status has changed
  if (isTrayEmpty != prevIsTrayEmpty) {
    sendTrayEmptyStatus();
    prevIsTrayEmpty = isTrayEmpty;
  }
}
