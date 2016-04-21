/*
    Group C4, MP2
    This program controls a device that automates an 'experiment.' Automates
    control of a salinity probe, bead hopper, water pump, and mixing impeller.

    Pins in use:
     2 - digital out when reading salinity
     4 - digital out after reading salinity
     9 - Control of the hopper servo
    12 - control of relay-2 for mixing impeller
    13 - control of relay-1 for water pump
    A1 - analog read for salinity
*/
#include <Servo.h>

bool debug = true;  // Flag for debugging purposes

String measurements[40];
int measurementID = 0;

Servo servo;  // Servo variable for the bead hopper
unsigned long startTime = -1; // Start time in millis
unsigned long lastMeasurement = -1; // Current time in millis
unsigned long lastMix = -1; // Time the last mix began

// Constants for y = ax^2 + bx + c (saline curve)
float a = 1.743E-10;
float b = 4.865E-5;
float c = 0.0;
// Constants for calibration
float C1 = 1.0;
float C2 = 0.0;
// "Experiment" parameters
float temperature = 21.11;

/*
   Convert resistivity at 25C to weight percent based on established data
*/
float weightPercent(float resistivity) {
  float x = 1000000 / resistivity;
  return a * x * x + b * x + c;
}

/*
   Calculate resistivity from measured resistance
*/
float resistivity(float salineResistance) {
  return (salineResistance * C1 * ((temperature + 21.5) / 46.5)) + C2;
}

/*
   Take a salinity measurement
   Function will only run once per second, regardless of when calls are made
*/
float measureSalinity() {
  float wtPercent = -1;
  int timeDelay = millis() - lastMeasurement; // Has it been 1 second since the last measurement?
  if (timeDelay >= 1000) { // If so, take a measurement
    lastMeasurement = millis();
    pinMode(2, OUTPUT);
    delay(1);
    digitalWrite(2, HIGH);
    delay(1);
    int probeReading = analogRead(A1);
    digitalWrite(2, LOW);
    pinMode(2, INPUT);
    delay(1);
    digitalWrite(4, HIGH);
    delay(1);
    digitalWrite(4, LOW);

    float probe = probeReading * 5.0;
    float Vout = probe / 1023.0;
    probe = (5.0 / Vout) - 1.0;
    float saline = 330 * probe;
    float rho = resistivity(saline);
    float microMho = 1.0 / rho;
    wtPercent = weightPercent(rho);

    if (debug) {
      // Terminal formatting:
//      Serial.print("Vout: ");
//      Serial.print(Vout);
//      Serial.print("V, R: ");
//      Serial.print(saline);
//      Serial.print(" ohms, Resistivity: ");
//      Serial.print(rho);
//      Serial.print(" ohms-cm, Conductivity: ");
//      Serial.print(microMho);
//      Serial.print(" micromhos, Wt%: ");
//      Serial.print(wtPercent);
//      Serial.println("%");
      // CSV formatting:
      Serial.print(Vout);
      Serial.print(",");
      Serial.print(saline);
      Serial.print(",");
      Serial.print(rho);
      Serial.print(",");
      Serial.print(microMho);
      Serial.print(",");
      Serial.println(wtPercent);
    }
  }
  return wtPercent;
}

/*
   Prompt the user to verify a String message
*/
bool verify(String msg) {
  Serial.println("");
  Serial.print(msg + " (y/n): ");
  char response = 'a';
  do {
    if (Serial.available()) {
      response = Serial.read();
      if (response != 'n' && response != 'y') {
        Serial.println("");
        Serial.println("Invalid response. Please enter 'y' for yes or 'n' for no: ");
        Serial.print(msg + " (y/n): ");
      }
    }
    delay(10);
  } while (response != 'n' && response != 'y');
  Serial.println("");
  return response == 'y';
}

/*
   Promps user for input with msg, then verifies their response with
   the String verifyMsgPrefix + input + verifyMsgPostfix
*/
float getFloatInput(String msg, String verifyMsgPrefix, String verifyMsgPostfix) {
  Serial.print(msg);
  float input = -1.0;
  while (input < 0) {
    if (Serial.available() > 0) {
      input = Serial.parseFloat();
      String verifyMsg = verifyMsgPrefix + String(input) + verifyMsgPostfix;
      if (!verify(verifyMsg)) {
        input = -1.0;
        Serial.print(msg);
      }
    }
    delay(10);
  }
  Serial.println("");
  return input;
}

/*
   Calibrate the probe
*/
void calibrate() {
  String percents[] = {"20","16","12","8","4"};
  for (int i = 0; i < 5; i++) {
    Serial.println("Place probe in " + percents[i] + "% saline and enter any key");
    while(Serial.available() == 0) {
      delay(10);
    }
    for (int j = 0; j < 5; j++) {
      pinMode(2, OUTPUT);
      delay(1);
      digitalWrite(2, HIGH);
      delay(1);
      int probeReading = analogRead(A1);
      digitalWrite(2, LOW);
      pinMode(2, INPUT);
      delay(1);
      digitalWrite(4, HIGH);
      delay(1);
      digitalWrite(4, LOW);
      delay(995);
      Serial.print(probeReading + ",");
    }
    Serial.println("");
  }
}

/*
   Initialize all of the components, get experiment parameters,
   and begin the experiment by dispensing all necessary materials.
*/
void setup() {
  // Initialize components
  Serial.begin(9600);
  pinMode(2, INPUT);  // Control for current through probe when reading
  pinMode(4, OUTPUT); // Control for current through probe after reading
  pinMode(13, OUTPUT);  // Control of relay-1 for water pump
  digitalWrite(13, HIGH); // Ensure relay-1 is off
  pinMode(12, OUTPUT);  // Control of relay-2 for mixing impeller
  digitalWrite(12, HIGH); // Ensure relay-2 is off
  servo.attach(9); // Attach the servo controller to the servo
  // Move the servo to ensure it is in the correct starting position
  servo.write(0);
  delay(1000);
  servo.write(180);
  delay(1000);
  servo.write(0);
  delay(1000);

  // Get experiment parameters from the user
  String msg = "Enter initial volume of DI water in mixing tank (in mL): ";
  String verifyMsgA = "";
  String verifyMsgB = "mL of DI water is in the mixing tank, is that correct?";
  int mL_water = (int) getFloatInput(msg, verifyMsgA, verifyMsgB);

  msg = "Enter volume of beads to dispense (in mL): ";
  verifyMsgA = "";
  verifyMsgB = "mL of beads will be dispensed, is that correct?";
  int volume_of_beads = (int) getFloatInput(msg, verifyMsgA, verifyMsgB);

  msg = "Enter target salinity (in weight percent): ";
  verifyMsgA = "Target salinity = ";
  verifyMsgB = "%, is that correct?";
  float target_salinity = getFloatInput(msg, verifyMsgA, verifyMsgB);

  msg = "Enter temperature of saline solution (in degrees C): ";
  verifyMsgA = "Temperature = ";
  verifyMsgB = "C, is that correct?";
  temperature = getFloatInput(msg, verifyMsgA, verifyMsgB);

  /*
     Calculate volume of saline solution to pump into the mixing tank.

     Beads lose just under 40% (2/5) of their water after 10 mihutes, and 50mL of 
     beads displaces just over 25mL of water (1/2), so the volume of distilled 
     water in the solution at 10 minutes is volume_of_beads / 5 + mL_water.

     2 * target salinity / 26 returns the percent of saline solution needed to
     reach the target salinity (target_salinity / 13).
  */
  int mL_saline = (volume_of_beads / 5 + mL_water) * target_salinity / 13;
  // pump time = 1.5 seconds + (volume of saline to pump / 200mL per 2.25 seconds)
  unsigned long pumpTime = 1500 + (mL_saline / 200 * 2250);

  float beads_dispensed = 0;
  unsigned long last_dispense = -1;
  bool dispensing = true;
  bool pumping = true;
  bool servo_opening = true;

  Serial.println("");
  Serial.println("Enter any key to begin");
  Serial.println("");
  while (Serial.available() <= 0) {
    delay(10);
  }

  digitalWrite(13, LOW);  // begin pumping saline solution

  unsigned long currentTime = millis(); // Get the time
  startTime = currentTime; // Set the starting time
  pumpTime += startTime; // Pump will stop at time pumpTime + start time (startTime)

  // Dispense all of the necessary materials
  while (pumping || dispensing) {

    // Check if we need to turn off the pump
    if (pumping && currentTime >= pumpTime) {
      pumping = false;
      digitalWrite(13, HIGH); // Turn off the pump
    }

    // Check if we need to turn the mixer on or off
    if (!pumping && lastMix < 0) {
      digitalWrite(12, LOW);  // Turn on the mixer
      lastMix = millis();
    } else if (!pumping && currentTime >= lastMix + 30000) {
      digitalWrite(12, HIGH);  // Turn off the mixer
    }

    // Check if we need to manipulate the hopper's servo
    if (currentTime >= last_dispense + 3000) {
      servo.write(180); // Open the servo
      servo_opening = true; // Indicate we are opening the servo
      last_dispense = millis();  // Mark the time we started dispensing beads
    } else if (servo_opening && currentTime >= last_dispense + 1200) {
      servo.write(0); // Close the servo
      servo_opening = false;  // Indicate we are closing the servo
      beads_dispensed += 50;  // Add 50mL to the total beads dispensed
      if (beads_dispensed >= volume_of_beads) {  // Check if all beads have been dispensed
        dispensing = false;
      }
    }
    currentTime = millis();
  }

  while (millis() < lastMix + 30000) {
    delay(1);
  }
  digitalWrite(12, HIGH);  // Turn off the mixer
  delay(5000);
  lastMix = millis() - 15000;
  
  String id = String(measurementID,DEC);
  measurements[measurementID] = id;
}

/*
   Run the experiment
*/
void loop() {
  unsigned long currentTime = millis();
  if (currentTime < 600000 + startTime) {
    if (currentTime >= lastMix + 20000) {
      digitalWrite(12, LOW);  // Turn on the mixer
      lastMix = currentTime;
      measurementID += 1;
      measurements[measurementID] = String(measurementID,DEC);
    } else if (currentTime >= lastMix + 15000) {
      // Try to take a measurement
      float current_salinity = measureSalinity();
      
      if (current_salinity >= 0) {  // If the measurement is significant, print it
        measurements[measurementID].concat(",");
        measurements[measurementID].concat(current_salinity);
        Serial.println(current_salinity);
      }
    } else if (currentTime >= lastMix + 10000) {
      digitalWrite(12, HIGH);  // Turn off the mixer
    }
  } else if (measurementID > 0) {
    Serial.println("ID,Measurement1,Measurement2,Measurement3,Measurement4,Measurement5");
    for (int i = 0; i < measurementID; i++) {
      Serial.println(measurements[i]);
    }
    measurementID = -1;
  } else {
    digitalWrite(12, HIGH);  // Turn off the mixer
    while(1) {
      delay(1000);
    } 
  }
}


