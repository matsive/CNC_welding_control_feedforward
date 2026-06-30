// Stepper motor pins (A4988 driver)
const int dirPin = 5;             // Direction pin
const int stepPin = 6;            // Step pin

// Buttons
const int modeButton = 8;         // Button to switch AUTO / MANUAL
const int extrudeButton = 9;      // Button to extrude in MANUAL
const int retractButton = 10;      // Button to retract in MANUAL

// RGB LED pins (common cathode, connected to GND)
const int ledR = A0;              // Red pin
const int ledG = A1;              // Green pin
const int ledB = A2;              // Blue pin

// Mode variable
bool autoMode = true;  // start in AUTO mode

void setup() {

  pinMode(dirPin, OUTPUT);
  pinMode(stepPin, OUTPUT);
  digitalWrite(dirPin, LOW);
  digitalWrite(stepPin, LOW);

  pinMode(modeButton, INPUT_PULLUP);     // button uses pull-up
  pinMode(extrudeButton, INPUT_PULLUP);  // button uses pull-up
  pinMode(retractButton, INPUT_PULLUP);  // button uses pull-up

  pinMode(ledR, OUTPUT);
  pinMode(ledG, OUTPUT);
  pinMode(ledB, OUTPUT);

  updateLED();  // show initial mode color
  Serial.begin(9600);
}

// Helper function: step motor
void stepMotor(int steps, bool dir, int stepDelay) {
  digitalWrite(dirPin, dir);  // true = forward, false = reverse
  for (int i = 0; i < steps; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(stepDelay);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(stepDelay);
  }
}

// Update LED based on mode
void updateLED() {
  if (autoMode) {
    // AUTO = Red
    digitalWrite(ledR, HIGH);
    digitalWrite(ledG, LOW);
    digitalWrite(ledB, LOW);
  } else {
    // MANUAL = Cyan (Green + Blue)
    digitalWrite(ledR, LOW);
    digitalWrite(ledG, HIGH);
    digitalWrite(ledB, HIGH);
  }
}

void loop() {
  // Check mode button (toggle on press)
  static bool lastModeButton = HIGH;
  bool currentModeButton = digitalRead(modeButton);
  if (lastModeButton == HIGH && currentModeButton == LOW) {
    autoMode = !autoMode;   // toggle mode
    updateLED();
    delay(300); // debounce
  }
  lastModeButton = currentModeButton;

  if (autoMode) {
    // --- AUTO MODE ---
    stepMotor(20, true, 15000);
  } else {
    // --- MANUAL MODE ---
    if (digitalRead(extrudeButton) == LOW) {
      Serial.println("Manual extrude pressed; Stepper forward");
      stepMotor(20, true, 800);  // forward extrusion
    }
    if (digitalRead(retractButton) == LOW) {
      Serial.println("Manual retract pressed; Stepper backward");
      stepMotor(20, false, 800); // reverse extrusion
    }
  }
}
