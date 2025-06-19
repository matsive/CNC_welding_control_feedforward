const int detectPin = A7;         // Analog pin to measure voltage
const int detectorPin2 = 2;       // Digital pin to detect logic HIGH
const int weldingPin = 7;         // Output pin to control welder

void setup() {
  pinMode(weldingPin, OUTPUT);
  pinMode(detectorPin2, INPUT);  // Set digital pin 2 as input
  digitalWrite(weldingPin, LOW); // Ensure welder is off at start
  Serial.begin(9600);
}

void loop() {
  long sumAnalog = 0;

  // Take 10 readings spaced 100 ms apart (total ~1000 ms)
  for (int i = 0; i < 50; i++) {
    sumAnalog += analogRead(detectPin);
    delay(10);
  }

  float averageAnalog = sumAnalog / 50;
  float averageVoltage = averageAnalog * (5.0 / 1023.0);
  int detectorState = digitalRead(detectorPin2); // Read digital pin 2

  Serial.print("Average Voltage: ");
  Serial.print(averageVoltage);
  Serial.print(" V; Detector Pin 2: ");
  Serial.print(detectorState ? "HIGH" : "LOW");
  Serial.print("; ");

  if (averageVoltage > 4.20 && detectorState == HIGH) {
    Serial.println("50V is ON; Welder ON");
    digitalWrite(weldingPin, HIGH);
  } else {
    Serial.println("50V is OFF; Welder OFF");
    digitalWrite(weldingPin, LOW);
  }

  // No extra delay needed here since the loop already takes ~1000 ms
}
