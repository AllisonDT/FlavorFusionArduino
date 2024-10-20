//Code to test motors on Ramps shield
//Hard-coded input is:
// 1:0.167;2:0.5;7:0.021;11:0;
//final spice container is 11 to reset to starting position

//***Research sleep mode***
//***Research TMC2209 wiring and programming***

const int totalSteps = 200; //200 steps per revolution
const int totalMicrosteps = 3200; //1/16 microstepping: 3200 steps per revolution
const int totalSpices = 10;

//NEMA 11 for rail. Connected to X-step and X-dir
#define railStep A0
#define railDir  A1

//NEMA 17 for carriage/ lazy susan. Connected to Y-step and Y-dir
#define susanStep A6
#define susanDir A7

//NEMA 8 for auger. Connected to Z-step and Z-dir
#define augerStep 46
#define augerDir 48

bool isSusanRotated = 0;
bool isRailForward = 0;
bool isOrderMixed = 1;

String incomingString = "";
String dataBuffer = "";

String spiceArray[10][2];
int spiceIndex[10][2];

int hardSpiceNumbers[] = {1, 2, 7, 11};
float hardSpiceAmounts[] = {0.167, 0.5, 0.021, 0.0};

int numSpicesOrdered;


void setup() {
  //Set analog pins to output
  pinMode(railStep, OUTPUT);
  pinMode(railDir, OUTPUT);
  pinMode(susanStep, OUTPUT);
  pinMode(susanDir, OUTPUT);

  //Begin comms
  Serial.begin(9600); //***switch to Bluetooth comm***
}

void loop() {
  //Receive spice data
  if (Serial.available()) { //***Change to bluetooth***
    while (Serial.available()){

      incomingString = Serial.readStringUntil('\n'); //read in spice order. \n from Serial Monitor

      //loop thru spice array and parse (turn this into a function instead???)
      int prevInd = 0; //index of previous semicolon + 1
      for (int i=0; i<10; i++){ 

        //break loop if no colon detected (end of string reached)
        if(incomingString.indexOf(':', prevInd) == -1){
          break;
        }
        spiceIndex[i][0] = incomingString.indexOf(':', prevInd); //spice number comes before colon
        spiceIndex[i][1] = incomingString.indexOf(';', spiceIndex[i][0]); //spice amount comes before semicolon
        spiceArray[i][0] = incomingString.substring(prevInd, spiceIndex[i][0]); //parse spice number
        spiceArray[i][1] = incomingString.substring(spiceIndex[i][0]+1, spiceIndex[i][1]); //parse spice amount
        prevInd = spiceIndex[i][1] + 1;
        numSpicesOrdered = i;
      }
    }
    isOrderMixed = 0;

    //for debugging: print parsed strings
    Serial.print("Spice number   ");
    Serial.println("Amount (oz)");    
    for (int j=0; j<=numSpicesOrdered; j++) {
      Serial.print(spiceArray[j][0]);
      Serial.print("   ");
      Serial.println(spiceArray[j][1]);
    }
  }

  //Process spice data 
  //***need to organize spice containers ascending!!!***
  //***Need to convert string data to int (container number) and float (spice amount)

  if(isOrderMixed == 0){
    //Calibrate susan
    calibrate();

    //Loop through all requested spices
    for (int j=0; j<=numSpicesOrdered; j++){

      //Move susan to requested spice
      moveSusan(j);

      //Move rail
      moveRailForward();

      //Move auger for requested amount
      moveAuger(j);

      //Move rail back
      moveRailBackward();
    }
    //Recalibrate
    calibrate();  
    isOrderMixed = 1; //Finish job
  }

}



void calibrate(){
  isSusanRotated = 0;
}

void moveSusan(int j){
  Serial.println("Moving spice carriage");

  //find starting position based on previously dispensed spice
  int prevSpice;
  if (j==0){
    prevSpice = 1; //assume fully calibrated for first spice
  }
  else {
    prevSpice = hardSpiceNumbers[j-1];
  }

  //calculate steps to desired spice
  int spiceDiff = abs(hardSpiceNumbers[j] - prevSpice); //change according to container numbers
  int numSteps = spiceDiff/totalSpices * totalSteps;

  digitalWrite(susanDir, LOW); //susan only moves in one direction
  for(int s=0; s<numSteps; s++){
    digitalWrite(susanStep,HIGH); 
    delayMicroseconds(500); //tune delays for speed, maybe PID?
    digitalWrite(susanStep,LOW); 
    delayMicroseconds(500); 
  }

  Serial.println("Spice carriage motion complete");
  Serial.print("Rotated to container mumber ");
  Serial.println(hardSpiceNumbers[j]);
  Serial.println();
  isSusanRotated = 1;
  delay(1000);
}

void moveRailForward(){
  Serial.println("Moving rail forward");
  
  int numSteps = 20*totalSteps; //20 revolutions is about 0.75 inches

  digitalWrite(railDir, LOW); //***make sure this is the right direction to spin forward***
  for(int s=0; s<numSteps; s++){
    digitalWrite(railStep,HIGH); 
    delayMicroseconds(500); //tune delays for speed, maybe PID?
    digitalWrite(railStep,LOW); 
    delayMicroseconds(500); 
  }  

  Serial.println("Rail forward motion complete\n");
  isRailForward = 1;
  delay(1000);
}

void moveRailBackward(){
  Serial.println("Moving rail backward");
  
  int numSteps = 20*totalSteps; //20 revolutions is about 0.75 inches

  digitalWrite(railDir, HIGH); //***make sure this is the right direction to spin forward***
  for(int s=0; s<numSteps; s++){
    digitalWrite(railStep,HIGH); 
    delayMicroseconds(500); //tune delays for speed, maybe PID?
    digitalWrite(railStep,LOW); 
    delayMicroseconds(500); 
  }  

  Serial.println("Rail backward motion complete\n");
  isRailForward = 0;
  delay(1000);
}

void moveAuger(int j){
  Serial.println("Moving auger");

  //calculate steps for desired spice amount *** will need to tune this!***
  int revPerOz = 20;
  int numSteps = hardSpiceAmounts[j] * revPerOz * totalSteps; //amount (oz) * revs per oz * steps per rev

  digitalWrite(augerDir, LOW); //***make sure this is the right direction to dispense spice***
  for(int s=0; s<numSteps; s++){
    digitalWrite(augerStep,HIGH); 
    delayMicroseconds(500); //tune delays for speed, maybe PID?
    digitalWrite(augerStep,LOW); 
    delayMicroseconds(500); 
  }

  //Need to ensure auger exits at specified angle

  //Maybe print out how much of the spice has been dispensed
  Serial.println("Auger motion complete");
  Serial.print("Dispensed ");
  Serial.print(hardSpiceAmounts[j]);
  Serial.println(" ounces of spice");
  Serial.println();
  delay(1000);
}
