/*
    Group C4, MP2
    This program controls a device that automates an 'experiment.' Automates
    control of a salinity probe, bead hopper, water pump, and mixing impeller.

    Resistivity: (salineResistance * ((temperature + 21.5) / 46.5));
    Salinity: 0.0000002074 * pow(2.71828,(float)rawRead*0.056);

    Pins in use:
     3 - digital out when reading salinity
     5 - digital out after reading salinity
     9 - Control of the hopper servo
    12 - control of relay-2 for mixing impeller
    13 - control of relay-3 for water pump
    A1 - analog read for salinity measurements
*/
#include <Servo.h>

Servo servo;  // Servo variable for the bead hopper
unsigned long startTime = -1; // Start time in millis
unsigned long lastMeasurement = -1; // Current time in millis
long lastMix = 0; // Time the last mix began
unsigned int salinity_average = 0;  // Average of 3 consecutive measurements
int count_measurements = 0; // Count to 3 consecutive measurements

/*
   Take a salinity measurement
   Function will only run once per second, regardless of when calls are made
*/
int measureSalinity() {
  int probeReading = -1;
  int timeDelay = millis() - lastMeasurement; // Has it been 1 second since the last measurement?
  if (timeDelay >= 1000) { // If so, take a measurement
    lastMeasurement = millis();
    pinMode(3, OUTPUT);
    delay(1);
    digitalWrite(3, HIGH);
    delay(1);
    probeReading = analogRead(A1);
    digitalWrite(3, LOW);
    pinMode(3, INPUT);
    delay(1);
    digitalWrite(5, HIGH);
    delay(1);
    digitalWrite(5, LOW);
  }
  return probeReading;
}

/*
   Initialize all of the components, get experiment parameters,
   and begin the experiment by dispensing all necessary materials.
*/
void setup() {
  // Initialize components
  Serial.begin(9600);
  pinMode(3, INPUT);  // Control for current through probe when reading
  pinMode(5, OUTPUT); // Control for current through probe after reading
  pinMode(13, OUTPUT);  // Control of relay-1 for water pump
  digitalWrite(13, LOW); // Ensure relay-1 is off
  pinMode(12, OUTPUT);  // Control of relay-2 for mixing impeller
  digitalWrite(12, LOW); // Ensure relay-2 is off
  servo.attach(9); // Attach the servo controller to the servo
  // Move the servo to ensure it is in the correct starting position
  servo.write(0);
  delay(1000);
  servo.write(180);
  delay(1000);
  servo.write(0);
  delay(1000);

  // Wait for experiment parameters from serial input
  while(!Serial.available()) {
    delay(10);
  }
  unsigned int volume_of_beads = Serial.parseInt();
  // If we aren't dispensing beads, we are calibrating
  while (volume_of_beads == 0) {
    int current_salinity = measureSalinity();
    if (current_salinity > 0) {
      salinity_average += current_salinity;
      count_measurements += 1;
    }
    if (count_measurements == 3) {
      salinity_average /= count_measurements;
      Serial.println(salinity_average);
      salinity_average = 0;
      count_measurements = 0;
    }
    delay(1500);
  }
  Serial.println(volume_of_beads);  // ACK the bead volume
  // We aren't calibrating, so get the remaining variables
  while(Serial.available()) { // Clear serial data
    Serial.read();
  }
  while(!Serial.available()) {  // Wait for serial data
    delay(10);
  }
  float mL_saline = Serial.parseInt();
  Serial.println(mL_saline);  // ACK the saline solution volume
  while(Serial.available()) { // Clear serial data
    Serial.read();
  }
  // pump time = 1.5 seconds + (volume of saline to pump / 200mL per 2.25 seconds)
  unsigned long pumpTime = 1500 + ((((float)mL_saline) / 200.0) * 2250.0);

  float beads_dispensed = 0;  // Beads dispensed so far
  unsigned long last_dispense = -1; // Time of last dispense
  bool dispensing = true; // Flag indicating dispensing status
  bool pumping = true;  // Flag indicating saline pump status
  bool servo_opening = true;  // Flag indicating servo status

  digitalWrite(13, HIGH);  // begin pumping saline solution

  unsigned long currentTime = millis(); // Get the time
  startTime = currentTime; // Set the starting time
  pumpTime += startTime; // Pump will stop at time pumpTime + start time (startTime)
  
  // Dispense all of the necessary materials
  while (pumping || dispensing) {

    // Check if we need to turn off the pump
    if (pumping && currentTime >= pumpTime) {
      pumping = false;
      digitalWrite(13, LOW); // Turn off the pump
      digitalWrite(12, HIGH);  // Turn on the mixer
    }

    // Check if we need to turn the mixer on or off
    if (!pumping && lastMix == 0) {
      digitalWrite(12, HIGH);  // Turn on the mixer
      lastMix = millis();
    } else if (!pumping && currentTime >= lastMix + 30000) {
      digitalWrite(12, LOW);  // Turn off the mixer
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
  digitalWrite(12, LOW);  // Turn off the mixer
  delay(5000);
  lastMix = millis() - 20000;
}

/*
   Run the experiment
*/
void loop() {
  unsigned long currentTime = millis(); // Get the current time in ms
  if (currentTime < 600000 + startTime) { // Run for a total of 10 minutes
    
    // Turn on the mixer every 20 seconds
    if (currentTime >= lastMix + 20000) {
      digitalWrite(12, HIGH);  // Turn on the mixer
      lastMix = currentTime;
      salinity_average /= count_measurements;
      Serial.println(salinity_average);
      salinity_average = 0;
      count_measurements = 0;

    // Take measurements 15 seconds after the start of the last mix
    } else if (currentTime >= lastMix + 15000) {
      // Try to take a measurement
      int current_salinity = measureSalinity();
      if (current_salinity > 0) {
        salinity_average += current_salinity;
        count_measurements += 1;
        delay(1500);
      }

    // Turn off the mixer after it runs for 10 seconds
    } else if (currentTime >= lastMix + 10000) {
      digitalWrite(12, LOW);  // Turn off the mixer
    }

  // Experiment is over
  } else {
    digitalWrite(12, LOW);  // Ensure the mixer is off
    Serial.println("END");  // Indicate that the experiment is over
    while(1) {  // Do nothing until Arduino is turned off
      delay(1000);
    } 
  }
}
