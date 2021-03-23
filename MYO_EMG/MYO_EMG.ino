
/**
 * @file    MYO_EMG
 * @author  Kira Wadden
 * @date    August 2018
 * @brief   Communicating between the Myo armband and ESP32 via BLE to receive EMG notifications
 * 
 * Edited by Caleb Tseng-Tham, January 2020
 */

#include <BLEDevice.h>

// The remote service we wish to connect to.
static BLEUUID serviceUUID("d5060001-a904-deb9-4748-2c7f4a124842");
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID("d5060401-a904-deb9-4748-2c7f4a124842");

// EMG service UUID
static BLEUUID    emgSUUID("d5060005-a904-deb9-4748-2c7f4a124842");
// EMG characteristic UUID 0
static BLEUUID    emgCUUID("d5060105-a904-deb9-4748-2c7f4a124842");
// EMG characteristic UUID 2
static BLEUUID    emgC2UUID("d5060305-a904-deb9-4748-2c7f4a124842");

static BLEAddress *pServerAddress;
static boolean doConnect = false;
static boolean connected = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;

// triggered will be true when the data from Myo is greater than 100 (experimental data)
bool triggered = false;

// Variables to do "smoothing" on data (Found by Caleb Tseng-Tham from: https://www.arduino.cc/en/Tutorial/Smoothing)
const int numReadings = 10;
int readings[numReadings];      // the readings from the analog input
int readIndex = 0;              // the index of the current reading
int total = 0;                  // the running total
int average = 0;                // the average
// End of Smoothing Variables 
int diffIndex = 0; // the index for the average array

int LED_pin = 22;

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    Serial.print("Notify callback for EMG Data Characteristic: ");
    Serial.println(pBLERemoteCharacteristic->getUUID().toString().c_str());
    for ( int i = 0; i < length; i ++)
    {
      Serial.print(-200);
      Serial.print(",");
      Serial.print(200);
      Serial.print(",");
      int avgArr[numReadings/2]; // new empty array for averages

      int data_readout = (int8_t)pData[i];
      total = total - readings[readIndex];
      readings[readIndex] = data_readout;
      total = total + readings[readIndex];
      
      readIndex = readIndex + 1;

      // If we're at the end of the array
      if (readIndex >= numReadings){
        readIndex = 0;
      }
      if (diffIndex >= numReadings/2){
        diffIndex = 0;
      }

      average = total / numReadings; 
      
      avgArr[diffIndex] = average;
      diffIndex = diffIndex + 1;
      
      Serial.println(average);
      if (sizeof(avgArr) > 1  && avgArr[diffIndex] - avgArr[diffIndex - 1] > 5) {
      // the average array must have more than one element to calculate the difference
      // one downside of this is that there needs to be at least two average values in order for this to happen
        triggered = true;
        digitalWrite(LED_pin, HIGH);
        break;
      }
      else{
        triggered = false;
        digitalWrite(LED_pin, LOW);
      }
      //Serial.print(" ");
    }
}

// Connects to a server at a certain BLEAddress
bool connectToServer(BLEAddress pAddress) {
    Serial.print("Forming a connection to ");
    Serial.println(pAddress.toString().c_str());
    
    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(" - Created client");

    // Connect to the remove BLE Server.
    pClient->connect(pAddress);
    Serial.println(" - Connected to server");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      return false;
    }
    Serial.println(" - Found our service");
    
    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUID.toString().c_str());
      return false;
    }
    Serial.println(" - Found our characteristic");

    // set sleep mode
    uint8_t sleepPkt[3] = {0x09, 0x01, 0x01};
    pRemoteCharacteristic->writeValue(sleepPkt, 3, true);
    delay(500);

    // set EMG mode to send filtered
    uint8_t emgPkt[5] = {0x01, 0x03, 0x02, 0x00, 0x00 }; 
    pRemoteCharacteristic->writeValue(emgPkt, 5, true);
    delay(500);

    const uint8_t notificationOn[] = {0x01, 0x00};

    // Obtain reference to EMG service UUID
    pRemoteService = pClient->getService(emgSUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(emgSUUID.toString().c_str());
      return false;
    }
    Serial.println(" - Found our EMG service");
    Serial.println(emgSUUID.toString().c_str());
    
// Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(emgCUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(emgCUUID.toString().c_str());
      return false;
    }
    Serial.println(" - Found our EMG characteristic");
    Serial.println(emgCUUID.toString().c_str());
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    pRemoteCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(emgC2UUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(emgC2UUID.toString().c_str());
      return false;
    }
    Serial.println(" - Found our EMG characteristic");
    Serial.println(emgC2UUID.toString().c_str());
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    pRemoteCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
}
/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(serviceUUID)) {

      // 
      Serial.print("Found our device!  address: "); 
      advertisedDevice.getScan()->stop();

      pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      doConnect = true;

    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks

  int findIndexInTriggerPattern(int index){
    if (index >= 0) return index;
    else {
      return 3 + index;
    }
  }

void setup() {
  // Setup of LED
  pinMode(LED_pin, OUTPUT);
  
  
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 30 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);

  // Initialize Smoothing to zero
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }
} // End of setup.


// This is the Arduino main loop functio
void loop() {

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    Serial.println("doConnect is true!");
    if (connectToServer(*pServerAddress)) {
      Serial.println("We are now connected to the BLE Server.");
      connected = true;
    } else {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    doConnect = false;
  }
  Serial.println("waiting...");
  delay(1000);
} // End of loop
