/*
  Arduino Nano + CL86T Serial Feeder Control
  
  Serial Monitor commands:
  EF100    = extrude feeder forward 100 steps
  RF100    = retract feeder reverse 100 steps
  SF800    = set feeder speed delay to 800 microseconds
  SF400    = faster feeder speed
  SF1200   = slower feeder speed
  Default feeder speed delay = 800 microseconds.
  Important:
  Smaller SF value = faster motor
  Bigger SF value  = slower motor
  Set Serial Monitor baud rate to 9600.
  Use "No line ending" or "Newline".
*/

// CL86T wiring:
// D6  -> CL86T PUL+
// D5  -> CL86T DIR+
// GND -> CL86T PUL- and DIR-

// Extruder stepper driver pins
const int extruderdirpin  = 5;
const int extrudersteppin = 6;

// Feeder speed delay
// This controls both HIGH and LOW delay of the step pulse
unsigned int feederDelayUs = 800;

void setup() {
  pinMode(extruderdirpin, OUTPUT);
  pinMode(extrudersteppin, OUTPUT);

  digitalWrite(extruderdirpin, LOW);
  digitalWrite(extrudersteppin, LOW);

  Serial.begin(9600);
  Serial.println("Extruder serial control ready");
  Serial.println("Commands:");
  Serial.println("EF100 = extrude feeder 100 steps");
  Serial.println("RF100 = retract feeder 100 steps");
  Serial.println("SF800 = set feeder speed delay to 800 us");
}

// Motor moving command
void moveExtruder(long steps, bool direction) {
  digitalWrite(extruderdirpin, direction ? HIGH : LOW);
  delayMicroseconds(10);

  for (long i = 0; i < steps; i++) {
    digitalWrite(extrudersteppin, HIGH);
    delayMicroseconds(feederDelayUs);

    digitalWrite(extrudersteppin, LOW);
    delayMicroseconds(feederDelayUs);
  }
}

void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.length() < 3) {
      Serial.println("Invalid command. Use EF100, RF100, or SF800");
      return;
    }

    String commandType = command.substring(0, 2);
    commandType.toUpperCase();

    long value = command.substring(2).toInt();

    if (value <= 0) {
      Serial.println("Invalid value. Example: EF100, RF100, or SF800");
      return;
    }

    if (commandType == "EF") {
      Serial.print("Extruding feeder ");
      Serial.print(value);
      Serial.print(" steps at feeder delay ");
      Serial.print(feederDelayUs);
      Serial.println(" us");

      moveExtruder(value, true);
    }

    else if (commandType == "RF") {
      Serial.print("Retracting feeder ");
      Serial.print(value);
      Serial.print(" steps at feeder delay ");
      Serial.print(feederDelayUs);
      Serial.println(" us");

      moveExtruder(value, false);
    }

    else if (commandType == "SF") {
      feederDelayUs = value;

      Serial.print("Feeder speed delay changed to ");
      Serial.print(feederDelayUs);
      Serial.println(" us");
    }

    else {
      Serial.println("Unknown command. Use EF100, RF100, or SF800");
    }
  }
}
