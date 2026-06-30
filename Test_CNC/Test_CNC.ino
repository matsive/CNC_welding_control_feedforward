/*
  Arduino Nano + CL86T CNC Serial Control

  Position commands:

  X100        = go to X position 100
  Y100        = go to Y position 100
  X100 Y100   = go to X100 and Y100 together

  X50         = if X is at 100, it moves back to 50
  X0          = go back to X position 0

  Z50         = go to Z position 50 independently
  Z0          = go back to Z position 0

  EF100       = feeder extrude 100 steps
  RF100       = feeder retract 100 steps
  SF800       = feeder speed delay, default 800 us

  HELP        = print commands and max limits

  Do not mix Z with X/Y.
*/

// Feeder motor pins
const int extruderdirpin  = 5;
const int extrudersteppin = 6;

// X axis motor pins
const int xdirpin  = 2;
const int xsteppin = 3;

// Y axis motor pins
const int ydirpin  = 4;
const int ysteppin = 7;

// Z axis motor pins
const int zdirpin  = 8;
const int zsteppin = 9;

// Speed delays
unsigned int feederDelayUs = 800;
unsigned int axisDelayUs   = 800;

// Current positions
long currentX = 0;
long currentY = 0;
long currentZ = 0;

// Max allowed positions
const long MAX_X = 10000;
const long MAX_Y = 10000;
const long MAX_Z = 5000;

// Direction settings
const bool X_POSITIVE_DIR = true;
const bool Y_POSITIVE_DIR = true;
const bool Z_POSITIVE_DIR = true;

void setup() {
  pinMode(extruderdirpin, OUTPUT);
  pinMode(extrudersteppin, OUTPUT);

  pinMode(xdirpin, OUTPUT);
  pinMode(xsteppin, OUTPUT);

  pinMode(ydirpin, OUTPUT);
  pinMode(ysteppin, OUTPUT);

  pinMode(zdirpin, OUTPUT);
  pinMode(zsteppin, OUTPUT);

  digitalWrite(extrudersteppin, LOW);
  digitalWrite(xsteppin, LOW);
  digitalWrite(ysteppin, LOW);
  digitalWrite(zsteppin, LOW);

  Serial.begin(9600);
  Serial.println(F("CNC position control ready"));

  printHelp();
}

void printHelp() {
  Serial.println();
  Serial.println(F("=== CNC Commands ==="));
  Serial.println(F("X100        = go to X position 100"));
  Serial.println(F("Y100        = go to Y position 100"));
  Serial.println(F("X100 Y100   = go to X100 and Y100 together"));
  Serial.println(F("X50         = go back to X position 50"));
  Serial.println(F("X0          = go back to X position 0"));
  Serial.println(F("Z50         = go to Z position 50 independently"));
  Serial.println(F("Z0          = go back to Z position 0"));
  Serial.println(F("EF100       = feeder extrude 100 steps"));
  Serial.println(F("RF100       = feeder retract 100 steps"));
  Serial.println(F("SF800       = set feeder speed delay"));
  Serial.println(F("HELP        = print this menu"));
  Serial.println();

  Serial.println(F("Rules:"));
  Serial.println(F("X, Y, Z are absolute positions."));
  Serial.println(F("X and Y can move together."));
  Serial.println(F("Z must be sent separately."));
  Serial.println(F("EF/RF are relative feeder steps."));
  Serial.println(F("SF is optional. Default feeder delay is 800 us."));
  Serial.println();

  Serial.println(F("Max limits:"));
  Serial.print(F("MAX_X = "));
  Serial.println(MAX_X);
  Serial.print(F("MAX_Y = "));
  Serial.println(MAX_Y);
  Serial.print(F("MAX_Z = "));
  Serial.println(MAX_Z);

  Serial.println(F("===================="));
  Serial.println();
}

void singleStep(int stepPin, unsigned int stepDelayUs) {
  digitalWrite(stepPin, HIGH);
  delayMicroseconds(stepDelayUs);

  digitalWrite(stepPin, LOW);
  delayMicroseconds(stepDelayUs);
}

void moveFeeder(long steps, bool direction) {
  digitalWrite(extruderdirpin, direction ? HIGH : LOW);
  delayMicroseconds(10);

  for (long i = 0; i < steps; i++) {
    singleStep(extrudersteppin, feederDelayUs);
  }
}

void moveZToPosition(long targetZ) {
  if (targetZ < 0 || targetZ > MAX_Z) {
    Serial.println(F("Error: Z position out of range"));
    return;
  }

  long zMove = targetZ - currentZ;
  if (zMove == 0) {
    Serial.println(F("Z already at target position"));
    return;
  }

  bool zDir = zMove > 0 ? Z_POSITIVE_DIR : !Z_POSITIVE_DIR;

  digitalWrite(zdirpin, zDir ? HIGH : LOW);
  delayMicroseconds(10);

  long zCount = abs(zMove);

  for (long i = 0; i < zCount; i++) {
    singleStep(zsteppin, axisDelayUs);
  }

  currentZ = targetZ;

  Serial.print(F("Current Z = "));
  Serial.println(currentZ);
}

void moveXYToPosition(long targetX, bool moveX, long targetY, bool moveY) {
  if (moveX && (targetX < 0 || targetX > MAX_X)) {
    Serial.println(F("Error: X position out of range"));
    return;
  }

  if (moveY && (targetY < 0 || targetY > MAX_Y)) {
    Serial.println(F("Error: Y position out of range"));
    return;
  }

  long xMove = moveX ? targetX - currentX : 0;
  long yMove = moveY ? targetY - currentY : 0;

  if (xMove == 0 && yMove == 0) {
    Serial.println(F("X/Y already at target position"));
    return;
  }

  bool xDir = xMove > 0 ? X_POSITIVE_DIR : !X_POSITIVE_DIR;
  bool yDir = yMove > 0 ? Y_POSITIVE_DIR : !Y_POSITIVE_DIR;

  digitalWrite(xdirpin, xDir ? HIGH : LOW);
  digitalWrite(ydirpin, yDir ? HIGH : LOW);
  delayMicroseconds(10);

  long xCount = abs(xMove);
  long yCount = abs(yMove);
  long maxSteps = max(xCount, yCount);

  for (long i = 0; i < maxSteps; i++) {
    if (i < xCount) digitalWrite(xsteppin, HIGH);
    if (i < yCount) digitalWrite(ysteppin, HIGH);

    delayMicroseconds(axisDelayUs);

    if (i < xCount) digitalWrite(xsteppin, LOW);
    if (i < yCount) digitalWrite(ysteppin, LOW);

    delayMicroseconds(axisDelayUs);
  }

  if (moveX) currentX = targetX;
  if (moveY) currentY = targetY;

  Serial.print(F("Current X = "));
  Serial.print(currentX);
  Serial.print(F("   Current Y = "));
  Serial.println(currentY);
}

long getAxisValue(String command, char axis, bool &found) {
  command.toUpperCase();
  axis = toupper(axis);

  int index = command.indexOf(axis);

  if (index == -1) {
    found = false;
    return 0;
  }

  found = true;

  int start = index + 1;
  int end = start;

  while (end < command.length()) {
    char c = command.charAt(end);

    if ((c >= '0' && c <= '9') || c == '-') {
      end++;
    } else {
      break;
    }
  }

  return command.substring(start, end).toInt();
}

void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toUpperCase();

    if (command == "HELP") {
      printHelp();
      return;
    }

    if (command.length() < 2) return;

    Serial.print(F("Received command: "));
    Serial.println(command);

    if (command.startsWith("EF")) {
      long steps = command.substring(2).toInt();
      if (steps > 0) moveFeeder(steps, true);
    }

    else if (command.startsWith("RF")) {
      long steps = command.substring(2).toInt();
      if (steps > 0) moveFeeder(steps, false);
    }

    else if (command.startsWith("SF")) {
      long value = command.substring(2).toInt();
      if (value > 0) {
        feederDelayUs = value;
        Serial.print(F("Feeder delay set to "));
        Serial.println(feederDelayUs);
      }
    }

    else {
      bool hasX, hasY, hasZ;

      long targetX = getAxisValue(command, 'X', hasX);
      long targetY = getAxisValue(command, 'Y', hasY);
      long targetZ = getAxisValue(command, 'Z', hasZ);

      if (hasZ && (hasX || hasY)) {
        Serial.println(F("Error: Z is independent. Send Z separately."));
        return;
      }

      if (hasZ) {
        moveZToPosition(targetZ);
      }

      else if (hasX || hasY) {
        moveXYToPosition(targetX, hasX, targetY, hasY);
      }

      else {
        Serial.println(F("Unknown command. Type HELP."));
      }
    }
  }
}
