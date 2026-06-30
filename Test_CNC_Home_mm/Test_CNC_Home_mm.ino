/*
  Arduino Nano + CL86T CNC Serial Control for TIG/DED

  Commands are in millimeters and mm/min:

  X100                   = go to X position 100 mm
  Y100                   = go to Y position 100 mm
  X100 Y100              = move X/Y together

  X100 Y0 F600           = move X/Y at 600 mm/min
  X100 Y0 E20 F600 EF300 = move X/Y at 600 mm/min while feeding wire at 300 mm/min
  X100 Y0 E-5 F600 EF300 = move X/Y at 600 mm/min while retracting wire at 300 mm/min

  E20 EF300              = feed wire 20 mm at 300 mm/min
  E-20 EF300             = retract wire 20 mm at 300 mm/min

  Z50 F200               = move Z to 50 mm at 200 mm/min
  Z0 F200                = move Z to 0 mm at 200 mm/min

  F600                   = set default travel speed for X/Y and Z to 600 mm/min
  XYF600                 = set default X/Y travel speed to 600 mm/min
  ZF200                  = set default Z speed to 200 mm/min
  EF300                  = set default extruder/wire speed to 300 mm/min

  HOME                   = home X, Y, Z
  HELP                   = print commands and settings

  Rules:
  - X/Y are absolute positions.
  - Z is absolute position and must be sent separately.
  - E is relative wire-feed amount.
  - F is travel speed for X/Y or Z.
  - EF is extruder/wire-feed speed.
  - Do not mix Z with X/Y/E.
*/

// Feeder / extruder motor pins
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

// Home switch pins
const int xHomeSwitchPin = 10;
const int yHomeSwitchPin = 11;
const int zHomeSwitchPin = 12;

// Driver setting
const float DRIVER_STEPS_PER_REV = 800.0;

// Mechanical movement per 1 motor revolution
const float X_MM_PER_REV = 40.0;  // GT2 belt, 20 tooth pulley
const float Y_MM_PER_REV = 40.0;  // GT2 belt, 20 tooth pulley
const float Z_MM_PER_REV = 8.0;   // common T8 lead screw
const float E_MM_PER_REV = 40.0;  // wire feeder roller travel per rev, change this

// Pulse timing
const unsigned int PULSE_WIDTH_US = 5;
const unsigned int MIN_STEP_INTERVAL_US = 50;

// Default feedrates in mm/min
float xyFeedrateMMmin = 600.0;
float zFeedrateMMmin  = 200.0;
float eFeedrateMMmin  = 300.0;

// Current positions in mm
float currentX = 0.0;
float currentY = 0.0;
float currentZ = 0.0;

// Max allowed positions in mm
const float MAX_X = 500.0;
const float MAX_Y = 500.0;
const float MAX_Z = 100.0;

// Homing safety limit in steps
const long HOMING_MAX_STEPS = 30000;

// Direction settings
const bool X_POSITIVE_DIR = true;
const bool Y_POSITIVE_DIR = true;
const bool Z_POSITIVE_DIR = true;
const bool E_POSITIVE_DIR = true;

float floatAbs(float value) {
  if (value < 0) return -value;
  return value;
}

void setup() {
  pinMode(extruderdirpin, OUTPUT);
  pinMode(extrudersteppin, OUTPUT);

  pinMode(xdirpin, OUTPUT);
  pinMode(xsteppin, OUTPUT);

  pinMode(ydirpin, OUTPUT);
  pinMode(ysteppin, OUTPUT);

  pinMode(zdirpin, OUTPUT);
  pinMode(zsteppin, OUTPUT);

  pinMode(xHomeSwitchPin, INPUT_PULLUP);
  pinMode(yHomeSwitchPin, INPUT_PULLUP);
  pinMode(zHomeSwitchPin, INPUT_PULLUP);

  digitalWrite(extrudersteppin, LOW);
  digitalWrite(xsteppin, LOW);
  digitalWrite(ysteppin, LOW);
  digitalWrite(zsteppin, LOW);

  Serial.begin(9600);
  Serial.println(F("TIG/DED CNC mm/min control ready"));

  printHelp();
}

long mmToSteps(float mm, float mmPerRev) {
  mm = floatAbs(mm);
  return (long)((mm * DRIVER_STEPS_PER_REV / mmPerRev) + 0.5);
}

float stepsPerMM(float mmPerRev) {
  return DRIVER_STEPS_PER_REV / mmPerRev;
}

unsigned long calculateStepIntervalUs(float moveDistanceMM, float feedrateMMmin, long steps) {
  if (moveDistanceMM <= 0 || feedrateMMmin <= 0 || steps <= 0) {
    return 800;
  }

  float moveTimeMin = moveDistanceMM / feedrateMMmin;
  float moveTimeUs = moveTimeMin * 60.0 * 1000000.0;

  unsigned long intervalUs = (unsigned long)(moveTimeUs / steps);

  if (intervalUs < MIN_STEP_INTERVAL_US) {
    intervalUs = MIN_STEP_INTERVAL_US;
  }

  return intervalUs;
}

float calculateMoveTimeSeconds(float moveDistanceMM, float feedrateMMmin) {
  if (moveDistanceMM <= 0 || feedrateMMmin <= 0) return 0.0;
  return (moveDistanceMM / feedrateMMmin) * 60.0;
}

void printHelp() {
  Serial.println();
  Serial.println(F("=== TIG/DED CNC Commands ==="));
  Serial.println(F("X100                   = go to X position 100 mm"));
  Serial.println(F("Y100                   = go to Y position 100 mm"));
  Serial.println(F("X100 Y100              = move X/Y together"));
  Serial.println(F("X100 Y0 F600           = move X/Y at 600 mm/min"));
  Serial.println(F("X100 Y0 E20 F600 EF300 = move X/Y at 600 mm/min, wire at 300 mm/min"));
  Serial.println(F("E20 EF300              = feed wire 20 mm at 300 mm/min"));
  Serial.println(F("E-20 EF300             = retract wire 20 mm at 300 mm/min"));
  Serial.println(F("Z50 F200               = move Z to 50 mm at 200 mm/min"));
  Serial.println(F("F600                   = set default travel speed for X/Y and Z"));
  Serial.println(F("XYF600                 = set default X/Y speed"));
  Serial.println(F("ZF200                  = set default Z speed"));
  Serial.println(F("EF300                  = set default extruder/wire speed"));
  Serial.println(F("HOME                   = home X, Y, Z"));
  Serial.println(F("HELP                   = print this menu"));
  Serial.println();

  Serial.println(F("Current speeds:"));
  Serial.print(F("XY travel F = "));
  Serial.print(xyFeedrateMMmin);
  Serial.println(F(" mm/min"));

  Serial.print(F("Z travel F  = "));
  Serial.print(zFeedrateMMmin);
  Serial.println(F(" mm/min"));

  Serial.print(F("E wire EF   = "));
  Serial.print(eFeedrateMMmin);
  Serial.println(F(" mm/min"));
  Serial.println();

  Serial.println(F("Steps/mm:"));
  Serial.print(F("X steps/mm = "));
  Serial.println(stepsPerMM(X_MM_PER_REV));

  Serial.print(F("Y steps/mm = "));
  Serial.println(stepsPerMM(Y_MM_PER_REV));

  Serial.print(F("Z steps/mm = "));
  Serial.println(stepsPerMM(Z_MM_PER_REV));

  Serial.print(F("E steps/mm = "));
  Serial.println(stepsPerMM(E_MM_PER_REV));
  Serial.println();

  Serial.println(F("Max limits:"));
  Serial.print(F("MAX_X = "));
  Serial.println(MAX_X);

  Serial.print(F("MAX_Y = "));
  Serial.println(MAX_Y);

  Serial.print(F("MAX_Z = "));
  Serial.println(MAX_Z);

  Serial.println(F("============================"));
  Serial.println();
}

bool homeSwitchPressed(int switchPin) {
  return digitalRead(switchPin) == LOW;
}

void pulsePin(int stepPin, unsigned long stepIntervalUs) {
  digitalWrite(stepPin, HIGH);
  delayMicroseconds(PULSE_WIDTH_US);

  digitalWrite(stepPin, LOW);

  if (stepIntervalUs > PULSE_WIDTH_US) {
    delayMicroseconds(stepIntervalUs - PULSE_WIDTH_US);
  }
}

void pulseSelectedPins(bool stepX, bool stepY, bool stepE) {
  if (stepX) digitalWrite(xsteppin, HIGH);
  if (stepY) digitalWrite(ysteppin, HIGH);
  if (stepE) digitalWrite(extrudersteppin, HIGH);

  delayMicroseconds(PULSE_WIDTH_US);

  if (stepX) digitalWrite(xsteppin, LOW);
  if (stepY) digitalWrite(ysteppin, LOW);
  if (stepE) digitalWrite(extrudersteppin, LOW);
}

void homeOneAxis(char axisName, int dirPin, int stepPin, int switchPin, bool positiveDir, float &currentPosition) {
  Serial.print(F("Homing "));
  Serial.println(axisName);

  bool homeDir = !positiveDir;

  digitalWrite(dirPin, homeDir ? HIGH : LOW);
  delayMicroseconds(10);

  long stepsMoved = 0;

  while (!homeSwitchPressed(switchPin)) {
    pulsePin(stepPin, 800);
    stepsMoved++;

    if (stepsMoved >= HOMING_MAX_STEPS) {
      Serial.print(F("Error: "));
      Serial.print(axisName);
      Serial.println(F(" home switch not reached"));
      return;
    }
  }

  currentPosition = 0.0;

  Serial.print(axisName);
  Serial.println(F(" homed. Position = 0 mm"));
}

void homeAllAxes() {
  Serial.println(F("Starting HOME"));

  homeOneAxis('X', xdirpin, xsteppin, xHomeSwitchPin, X_POSITIVE_DIR, currentX);
  homeOneAxis('Y', ydirpin, ysteppin, yHomeSwitchPin, Y_POSITIVE_DIR, currentY);
  homeOneAxis('Z', zdirpin, zsteppin, zHomeSwitchPin, Z_POSITIVE_DIR, currentZ);

  Serial.println(F("HOME complete"));
}

void moveEOnly(float eMM, float eSpeedMMmin) {
  long eSteps = mmToSteps(eMM, E_MM_PER_REV);

  if (eSteps == 0) {
    Serial.println(F("E amount is zero"));
    Serial.println(F("OK"));
    return;
  }

  eFeedrateMMmin = eSpeedMMmin;

  bool eDir = eMM > 0 ? E_POSITIVE_DIR : !E_POSITIVE_DIR;
  digitalWrite(extruderdirpin, eDir ? HIGH : LOW);
  delayMicroseconds(10);

  unsigned long eIntervalUs = calculateStepIntervalUs(floatAbs(eMM), eFeedrateMMmin, eSteps);

  Serial.print(F("E-only move: E = "));
  Serial.print(eMM);
  Serial.print(F(" mm   EF = "));
  Serial.print(eFeedrateMMmin);
  Serial.print(F(" mm/min   E steps = "));
  Serial.print(eSteps);
  Serial.print(F("   E step interval us = "));
  Serial.println(eIntervalUs);

  for (long i = 0; i < eSteps; i++) {
    pulsePin(extrudersteppin, eIntervalUs);
  }

  Serial.println(F("OK"));
}

void moveZToPosition(float targetZ, float zSpeedMMmin) {
  if (targetZ < 0 || targetZ > MAX_Z) {
    Serial.println(F("Error: Z position out of range"));
    return;
  }

  float zMoveMM = targetZ - currentZ;

  if (zMoveMM == 0) {
    Serial.println(F("Z already at target position"));
    Serial.println(F("OK"));
    return;
  }

  zFeedrateMMmin = zSpeedMMmin;

  bool zDir = zMoveMM > 0 ? Z_POSITIVE_DIR : !Z_POSITIVE_DIR;
  long zSteps = mmToSteps(zMoveMM, Z_MM_PER_REV);
  unsigned long zIntervalUs = calculateStepIntervalUs(floatAbs(zMoveMM), zFeedrateMMmin, zSteps);

  digitalWrite(zdirpin, zDir ? HIGH : LOW);
  delayMicroseconds(10);

  Serial.print(F("Z move to "));
  Serial.print(targetZ);
  Serial.print(F(" mm   F = "));
  Serial.print(zFeedrateMMmin);
  Serial.print(F(" mm/min   Z steps = "));
  Serial.print(zSteps);
  Serial.print(F("   Z step interval us = "));
  Serial.println(zIntervalUs);

  for (long i = 0; i < zSteps; i++) {
    if (zMoveMM < 0 && homeSwitchPressed(zHomeSwitchPin)) {
      currentZ = 0.0;
      Serial.println(F("Z home switch hit. Current Z = 0 mm"));
      Serial.println(F("OK"));
      return;
    }

    pulsePin(zsteppin, zIntervalUs);
  }

  currentZ = targetZ;

  Serial.print(F("Current Z mm = "));
  Serial.println(currentZ);
  Serial.println(F("OK"));
}

void moveXYEToPosition(
  float targetX,
  bool moveX,
  float targetY,
  bool moveY,
  float eMM,
  bool moveE,
  float xySpeedMMmin,
  float eSpeedMMmin
) {
  if (moveX && (targetX < 0 || targetX > MAX_X)) {
    Serial.println(F("Error: X position out of range"));
    return;
  }

  if (moveY && (targetY < 0 || targetY > MAX_Y)) {
    Serial.println(F("Error: Y position out of range"));
    return;
  }

  float xMoveMM = moveX ? targetX - currentX : 0.0;
  float yMoveMM = moveY ? targetY - currentY : 0.0;
  float eMoveMM = moveE ? eMM : 0.0;

  long xSteps = mmToSteps(xMoveMM, X_MM_PER_REV);
  long ySteps = mmToSteps(yMoveMM, Y_MM_PER_REV);
  long eSteps = mmToSteps(eMoveMM, E_MM_PER_REV);

  if (xSteps == 0 && ySteps == 0 && eSteps == 0) {
    Serial.println(F("Nothing to move"));
    Serial.println(F("OK"));
    return;
  }

  if (moveX || moveY) {
    xyFeedrateMMmin = xySpeedMMmin;
  }

  if (moveE) {
    eFeedrateMMmin = eSpeedMMmin;
  }

  bool xDir = xMoveMM >= 0 ? X_POSITIVE_DIR : !X_POSITIVE_DIR;
  bool yDir = yMoveMM >= 0 ? Y_POSITIVE_DIR : !Y_POSITIVE_DIR;
  bool eDir = eMoveMM >= 0 ? E_POSITIVE_DIR : !E_POSITIVE_DIR;

  if (xSteps > 0) digitalWrite(xdirpin, xDir ? HIGH : LOW);
  if (ySteps > 0) digitalWrite(ydirpin, yDir ? HIGH : LOW);
  if (eSteps > 0) digitalWrite(extruderdirpin, eDir ? HIGH : LOW);

  delayMicroseconds(10);

  float xyDistanceMM = sqrt((xMoveMM * xMoveMM) + (yMoveMM * yMoveMM));

  unsigned long xIntervalUs = calculateStepIntervalUs(xyDistanceMM, xyFeedrateMMmin, xSteps);
  unsigned long yIntervalUs = calculateStepIntervalUs(xyDistanceMM, xyFeedrateMMmin, ySteps);
  unsigned long eIntervalUs = calculateStepIntervalUs(floatAbs(eMoveMM), eFeedrateMMmin, eSteps);

  float xyTimeSec = calculateMoveTimeSeconds(xyDistanceMM, xyFeedrateMMmin);
  float eTimeSec  = calculateMoveTimeSeconds(floatAbs(eMoveMM), eFeedrateMMmin);

  Serial.print(F("XY/E move   Xtarget = "));
  Serial.print(moveX ? targetX : currentX);
  Serial.print(F("   Ytarget = "));
  Serial.print(moveY ? targetY : currentY);
  Serial.print(F("   E = "));
  Serial.print(eMoveMM);
  Serial.print(F(" mm   F = "));
  Serial.print(xyFeedrateMMmin);
  Serial.print(F(" mm/min   EF = "));
  Serial.print(eFeedrateMMmin);
  Serial.println(F(" mm/min"));

  Serial.print(F("XY distance = "));
  Serial.print(xyDistanceMM);
  Serial.print(F(" mm   XY time = "));
  Serial.print(xyTimeSec);
  Serial.print(F(" s   E time = "));
  Serial.print(eTimeSec);
  Serial.println(F(" s"));

  Serial.print(F("X steps = "));
  Serial.print(xSteps);
  Serial.print(F("   Y steps = "));
  Serial.print(ySteps);
  Serial.print(F("   E steps = "));
  Serial.println(eSteps);

  Serial.print(F("X interval us = "));
  Serial.print(xIntervalUs);
  Serial.print(F("   Y interval us = "));
  Serial.print(yIntervalUs);
  Serial.print(F("   E interval us = "));
  Serial.println(eIntervalUs);

  if (moveE && (moveX || moveY)) {
    if (eTimeSec > xyTimeSec) {
      Serial.println(F("Note: E feed time is longer than XY time, so wire feed may continue after XY stops."));
    } else if (eTimeSec < xyTimeSec) {
      Serial.println(F("Note: E feed time is shorter than XY time, so wire feed may finish before XY stops."));
    } else {
      Serial.println(F("E feed time matches XY time."));
    }
  }

  long xDone = 0;
  long yDone = 0;
  long eDone = 0;

  bool xStoppedByHome = false;
  bool yStoppedByHome = false;

  unsigned long startTime = micros();
  unsigned long nextX = startTime;
  unsigned long nextY = startTime;
  unsigned long nextE = startTime;

  while (xDone < xSteps || yDone < ySteps || eDone < eSteps) {
    unsigned long now = micros();

    if (xMoveMM < 0 && !xStoppedByHome && homeSwitchPressed(xHomeSwitchPin)) {
      currentX = 0.0;
      xDone = xSteps;
      xStoppedByHome = true;
      Serial.println(F("X home switch hit. Current X = 0 mm"));
    }

    if (yMoveMM < 0 && !yStoppedByHome && homeSwitchPressed(yHomeSwitchPin)) {
      currentY = 0.0;
      yDone = ySteps;
      yStoppedByHome = true;
      Serial.println(F("Y home switch hit. Current Y = 0 mm"));
    }

    bool stepX = false;
    bool stepY = false;
    bool stepE = false;

    if (xDone < xSteps && (long)(now - nextX) >= 0) {
      stepX = true;
      xDone++;
      nextX += xIntervalUs;
    }

    if (yDone < ySteps && (long)(now - nextY) >= 0) {
      stepY = true;
      yDone++;
      nextY += yIntervalUs;
    }

    if (eDone < eSteps && (long)(now - nextE) >= 0) {
      stepE = true;
      eDone++;
      nextE += eIntervalUs;
    }

    if (stepX || stepY || stepE) {
      pulseSelectedPins(stepX, stepY, stepE);
    }
  }

  if (moveX && !xStoppedByHome) currentX = targetX;
  if (moveY && !yStoppedByHome) currentY = targetY;

  Serial.print(F("Current X mm = "));
  Serial.print(currentX);
  Serial.print(F("   Current Y mm = "));
  Serial.println(currentY);

  Serial.println(F("OK"));
}

bool isAlphaChar(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool isValueStart(char c) {
  return (c >= '0' && c <= '9') || c == '-' || c == '.';
}

bool isValueChar(char c) {
  return (c >= '0' && c <= '9') || c == '-' || c == '.';
}

float getValueByPrefix(String command, String prefix, bool &found) {
  command.toUpperCase();
  prefix.toUpperCase();

  int searchFrom = 0;
  found = false;

  while (true) {
    int index = command.indexOf(prefix, searchFrom);

    if (index == -1) {
      return 0.0;
    }

    bool goodStart = false;

    if (index == 0) {
      goodStart = true;
    } else {
      char prev = command.charAt(index - 1);
      if (!isAlphaChar(prev)) {
        goodStart = true;
      }
    }

    int valueStart = index + prefix.length();

    if (goodStart && valueStart < command.length() && isValueStart(command.charAt(valueStart))) {
      int valueEnd = valueStart;

      while (valueEnd < command.length() && isValueChar(command.charAt(valueEnd))) {
        valueEnd++;
      }

      found = true;
      return command.substring(valueStart, valueEnd).toFloat();
    }

    searchFrom = index + 1;
  }
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

    if (command == "HOME") {
      homeAllAxes();
      Serial.println(F("OK"));
      return;
    }

    if (command.length() < 1) return;

    Serial.print(F("Received command: "));
    Serial.println(command);

    bool hasX, hasY, hasZ, hasE, hasF, hasEF, hasXYF, hasZF;

    float targetX = getValueByPrefix(command, "X", hasX);
    float targetY = getValueByPrefix(command, "Y", hasY);
    float targetZ = getValueByPrefix(command, "Z", hasZ);
    float eMM     = getValueByPrefix(command, "E", hasE);

    float fValue   = getValueByPrefix(command, "F", hasF);
    float efValue  = getValueByPrefix(command, "EF", hasEF);
    float xyfValue = getValueByPrefix(command, "XYF", hasXYF);
    float zfValue  = getValueByPrefix(command, "ZF", hasZF);

    if (hasF && fValue <= 0) {
      Serial.println(F("Error: F must be positive mm/min"));
      return;
    }

    if (hasEF && efValue <= 0) {
      Serial.println(F("Error: EF must be positive mm/min"));
      return;
    }

    if (hasXYF && xyfValue <= 0) {
      Serial.println(F("Error: XYF must be positive mm/min"));
      return;
    }

    if (hasZF && zfValue <= 0) {
      Serial.println(F("Error: ZF must be positive mm/min"));
      return;
    }

    if (hasXYF) {
      xyFeedrateMMmin = xyfValue;
      Serial.print(F("Default XY speed set to "));
      Serial.print(xyFeedrateMMmin);
      Serial.println(F(" mm/min"));
    }

    if (hasZF) {
      zFeedrateMMmin = zfValue;
      Serial.print(F("Default Z speed set to "));
      Serial.print(zFeedrateMMmin);
      Serial.println(F(" mm/min"));
    }

    if (hasEF) {
      eFeedrateMMmin = efValue;
      Serial.print(F("Default E wire speed set to "));
      Serial.print(eFeedrateMMmin);
      Serial.println(F(" mm/min"));
    }

    if (hasF && !hasX && !hasY && !hasZ && !hasE && !hasXYF && !hasZF && !hasEF) {
      xyFeedrateMMmin = fValue;
      zFeedrateMMmin = fValue;

      Serial.print(F("Default travel speed for XY and Z set to "));
      Serial.print(fValue);
      Serial.println(F(" mm/min"));
      Serial.println(F("OK"));
      return;
    }

    if (!hasX && !hasY && !hasZ && !hasE && (hasXYF || hasZF || hasEF)) {
      Serial.println(F("OK"));
      return;
    }

    if (hasZ && (hasX || hasY || hasE || hasEF || hasXYF)) {
      Serial.println(F("Error: Z is independent. Send Z separately without X/Y/E/EF/XYF."));
      return;
    }

    if (hasZ) {
      float zSpeed = hasZF ? zfValue : (hasF ? fValue : zFeedrateMMmin);
      moveZToPosition(targetZ, zSpeed);
    }

    else if (hasX || hasY || hasE) {
      if (hasE && !hasX && !hasY) {
        float eSpeed = hasEF ? efValue : eFeedrateMMmin;
        moveEOnly(eMM, eSpeed);
      } else {
        float xySpeed = hasXYF ? xyfValue : (hasF ? fValue : xyFeedrateMMmin);
        float eSpeed  = hasEF ? efValue : eFeedrateMMmin;

        moveXYEToPosition(targetX, hasX, targetY, hasY, eMM, hasE, xySpeed, eSpeed);
      }
    }

    else {
      Serial.println(F("Unknown command. Type HELP."));
    }
  }
}
