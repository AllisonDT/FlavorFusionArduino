#include <ArduinoBLE.h>

const int numContainers = 10;
int containerNumbers[numContainers] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
float spiceAmountsInGrams[numContainers] = {1.0, 4.5, 1.25, 2.8, 4.3, 3.6, 4.2, 3.9, 3.4, 2.7};

bool isOrderMixed = false;  // Track whether the whole order is mixed

BLEService spiceService("180C");
BLECharacteristic containerNumberCharacteristic("2A56", BLERead | BLENotify, sizeof(int32_t));
BLECharacteristic spiceAmountCharacteristic("2A57", BLERead | BLENotify, sizeof(float));
BLECharacteristic serializedIngredientsCharacteristic("2A58", BLEWrite | BLEWriteWithoutResponse, 512);
BLECharacteristic spiceMixedCharacteristic("2A59", BLERead | BLENotify, sizeof(uint8_t)); // Characteristic for order mixed status

void setup() {
  Serial.begin(9600);
  while (!Serial);

  if (!BLE.begin()) {
    Serial.println("Starting BLE failed!");
    while (1);
  }

  BLE.setLocalName("Flavor Fusion");
  BLE.setAdvertisedService(spiceService);
  spiceService.addCharacteristic(containerNumberCharacteristic);
  spiceService.addCharacteristic(spiceAmountCharacteristic);
  spiceService.addCharacteristic(serializedIngredientsCharacteristic);
  spiceService.addCharacteristic(spiceMixedCharacteristic); // Add the spice mixed characteristic
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
      // Send spice data as usual
      for (int i = 0; i < numContainers; i++) {
        sendSpiceData(containerNumbers[i], spiceAmountsInGrams[i]);
      }

      // Check if the entire order has been mixed
      if (isOrderMixed) {
        sendOrderMixedStatus(true);  // Send true when the order is done mixing
      }

      // Check for serial input to update spice amounts
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

void sendSpiceData(int containerNumber, float spiceAmount) {
  containerNumberCharacteristic.writeValue((int32_t)containerNumber);

  byte spiceAmountBytes[sizeof(float)];
  memcpy(spiceAmountBytes, &spiceAmount, sizeof(float));
  spiceAmountCharacteristic.writeValue(spiceAmountBytes, sizeof(spiceAmountBytes));
}

void sendOrderMixedStatus(bool mixedStatus) {
  uint8_t mixedStatusByte = mixedStatus ? 1 : 0;  // Convert the boolean to a byte
  spiceMixedCharacteristic.writeValue(mixedStatusByte);  // Send the byte value
  if (mixedStatus) {
    Serial.println("Order has been mixed!");
  }
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
  int separatorIndex = data.indexOf(';');
  
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
    markOrderAsMixed();

    start = separatorIndex + 1;
    separatorIndex = data.indexOf(';', start);
  }
}

void markOrderAsMixed() {
  isOrderMixed = true;  // Once all ingredients are processed, mark the order as mixed
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

  int spaceIndex = input.indexOf(' ');
  if (spaceIndex == -1) {
    Serial.println("Invalid input. Use format: containerNumber newAmount or 'complete'/'reset' to simulate blend completion.");
    return;
  }

  String containerStr = input.substring(0, spaceIndex);
  String amountStr = input.substring(spaceIndex + 1);

  int containerNumber = containerStr.toInt();
  float newAmount = amountStr.toFloat();

  // Update the spice array
  bool found = false;
  for (int i = 0; i < numContainers; i++) {
    if (containerNumbers[i] == containerNumber) {
      spiceAmountsInGrams[i] = newAmount;
      isOrderMixed = false;  // Reset the mixed status if any spice amount is updated
      found = true;
      Serial.print("Updated container ");
      Serial.print(containerNumber);
      Serial.print(" with new amount: ");
      Serial.println(newAmount);
      break;
    }
  }

  if (!found) {
    Serial.println("Container number not found.");
  }
}

