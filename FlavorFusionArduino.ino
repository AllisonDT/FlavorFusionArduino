#include <ArduinoBLE.h>

bool isOrderMixed = false;  // Track whether the whole order is mixed
bool isTrayEmpty = true;   // Track whether the tray is empty

BLEService spiceService("180C");

BLECharacteristic serializedIngredientsCharacteristic(
  "2A58", 
  BLEWrite | BLEWriteWithoutResponse, 
  512
);

BLECharacteristic spiceMixedCharacteristic(
  "2A59", 
  BLERead | BLENotify, 
  sizeof(uint8_t)
); // Characteristic for order mixed status

BLECharacteristic trayStatusCharacteristic(
  "19B10002-E8F2-537E-4F6C-D104768A1214", 
  BLERead | BLENotify, 
  sizeof(uint8_t)
); // Tray status characteristic

void setup() {
  Serial.begin(9600);
  while (!Serial);

  if (!BLE.begin()) {
    Serial.println("Starting BLE failed!");
    while (1);
  }

  BLE.setLocalName("Flavor Fusion");
  BLE.setAdvertisedService(spiceService);

  spiceService.addCharacteristic(serializedIngredientsCharacteristic);
  spiceService.addCharacteristic(spiceMixedCharacteristic);
  spiceService.addCharacteristic(trayStatusCharacteristic); // Add the tray status characteristic

  BLE.addService(spiceService);
  BLE.advertise();

  // Set the event handler for the writable characteristic
  serializedIngredientsCharacteristic.setEventHandler(BLEWritten, onIngredientsWritten);

  Serial.println("BLE device is ready and advertising.");
}

void loop() {
  BLEDevice central = BLE.central();

  if (central) {
    Serial.print("Connected to central: ");
    Serial.println(central.address());

    while (central.connected()) {
      // Check if the entire order has been mixed
      if (isOrderMixed) {
        sendOrderMixedStatus(true);  // Send true when the order is done mixing
      }

      // Send tray empty status
      sendTrayEmptyStatus(isTrayEmpty);

      // Check for serial input to update statuses
      if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        processSerialInput(input);
      }
    }

    Serial.print("Disconnected from central: ");
    Serial.println(central.address());
  }

  // Also check for serial input when not connected
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    processSerialInput(input);
  }
}

void sendOrderMixedStatus(bool mixedStatus) {
  uint8_t mixedStatusByte = mixedStatus ? 1 : 0;  // Convert the boolean to a byte
  spiceMixedCharacteristic.writeValue(mixedStatusByte);  // Send the byte value
}

void sendTrayEmptyStatus(bool emptyStatus) {
  uint8_t statusByte = emptyStatus ? 1 : 0;  // Convert the boolean to a byte
  trayStatusCharacteristic.writeValue(statusByte);  // Send the byte value
}

void onIngredientsWritten(BLEDevice central, BLECharacteristic characteristic) {
  int dataLength = characteristic.valueLength();

  String receivedData = "";
  for (int i = 0; i < dataLength; i++) {
    receivedData += (char)characteristic.value()[i];
  }

  Serial.print("Received serialized ingredients: ");
  Serial.println(receivedData);
  
  processReceivedIngredients(receivedData);
}

void processReceivedIngredients(String data) {
  Serial.println("Processing received ingredients data:");
  
  int start = 0;
  int separatorIndex = data.indexOf(';', start);
  
  while (separatorIndex != -1) {
    String ingredientPair = data.substring(start, separatorIndex);
    int colonIndex = ingredientPair.indexOf(':');
    String ingredientName = ingredientPair.substring(0, colonIndex);
    String ingredientAmount = ingredientPair.substring(colonIndex + 1);
    
    Serial.print("Ingredient: ");
    Serial.print(ingredientName);
    Serial.print(", Amount: ");
    Serial.println(ingredientAmount);
    
    // Here you would process each ingredient and mark it as mixed (simplified in this example)

    start = separatorIndex + 1;
    separatorIndex = data.indexOf(';', start);
  }

  // After processing all ingredients, mark the order as mixed
  markOrderAsMixed();
}

void markOrderAsMixed() {
  isOrderMixed = true;  // Once all ingredients are processed, mark the order as mixed
  sendOrderMixedStatus(true);  // Send the status immediately
  Serial.println("Order has been marked as mixed.");
}

void resetOrderMixed() {
  isOrderMixed = false;  // Reset the order mixed status
  sendOrderMixedStatus(false);  // Send a status of false to indicate the order is not mixed
  Serial.println("Order has been reset and marked as not mixed.");
}

void processSerialInput(String input) {
  input.trim();

  // Check if the input is to simulate the blend being completed
  if (input.equalsIgnoreCase("complete")) {
    markOrderAsMixed();  // Mark the blend as completed
    Serial.println("Manually marked the order as mixed.");
    return;
  }

  // Check if the input is to reset the blend completion status
  if (input.equalsIgnoreCase("reset")) {
    resetOrderMixed();  // Unmark the blend as completed
    Serial.println("Manually reset the order as not mixed.");
    return;
  }

  // Check if the input is to set the tray as empty
  if (input.equalsIgnoreCase("tray empty")) {
    isTrayEmpty = true;
    sendTrayEmptyStatus(true);
    Serial.println("Manually set tray as empty.");
    return;
  }

  // Check if the input is to set the tray as not empty
  if (input.equalsIgnoreCase("tray not empty")) {
    isTrayEmpty = false;
    sendTrayEmptyStatus(false);
    Serial.println("Manually set tray as not empty.");
    return;
  }

  Serial.println("Invalid input. Use 'complete'/'reset' or 'tray empty'/'tray not empty' to simulate actions.");
}
