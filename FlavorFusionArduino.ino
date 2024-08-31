#include <ArduinoBLE.h>

const int numContainers = 10;
int containerNumbers[numContainers] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
float spiceAmountsInGrams[numContainers] = {1.0, 1.5, 1.25, 2.8, 4.3, 3.6, 4.2, 3.9, 3.4, 2.7};

BLEService spiceService("180C");
BLECharacteristic containerNumberCharacteristic("2A56", BLERead | BLENotify, sizeof(int32_t));
BLECharacteristic spiceAmountCharacteristic("2A57", BLERead | BLENotify, sizeof(float));

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
  BLE.addService(spiceService);
  BLE.advertise();

  Serial.println("BLE device is ready and advertising.");
}

void loop() {
  BLEDevice central = BLE.central();

  if (central) {
    Serial.print("Connected to central: ");
    Serial.println(central.address());

    while (central.connected()) {
      for (int i = 0; i < numContainers; i++) {
        sendSpiceData(containerNumbers[i], spiceAmountsInGrams[i]);
      }
    }

    Serial.print("Disconnected from central: ");
    Serial.println(central.address());
  }
}

void sendSpiceData(int containerNumber, float spiceAmount) {
  containerNumberCharacteristic.writeValue((int32_t)containerNumber);

  byte spiceAmountBytes[sizeof(float)];
  memcpy(spiceAmountBytes, &spiceAmount, sizeof(float));
  spiceAmountCharacteristic.writeValue(spiceAmountBytes, sizeof(spiceAmountBytes));

  Serial.print("Sent container number: ");
  Serial.print(containerNumber);
  Serial.print(", spice amount: ");
  Serial.println(spiceAmount);
}
