/*
  Arduino Nano + CL86T CNC Serial Control for TIG/DED

  Commands are in millimeters and mm/min:

  X100                  = go to X position 100 mm
  Y100                  = go to Y position 100 mm
  X100 Y100             = move X/Y together

  X100 Y0 F600          = move X/Y at 600 mm/min

  E10                   = extrude exact 10 mm using default E speed
  E-10                  = retract exact 10 mm using default E speed
  E10 EF200             = extrude exact 10 mm at 200 mm/min
  E-10 EF200            = retract exact 10 mm at 200 mm/min

  X100 Y0 E10 F600      = move X/Y and extrude exact 10 mm during the move

  X100 Y0 EF200 F600    = move X/Y at 600 mm/min,
                           keep wire feeding at 200 mm/min until X/Y reaches target

  Z50 F200              = move Z to 50 mm at 200 mm/min
  Z0 F200               = move Z to 0 mm at 200 mm/min

  F600                  = set default travel speed for X/Y and Z
  XYF600                = set default X/Y speed
  ZF200                 = set default Z speed
  EF200                 = set default wire-feed speed

  HOME                  = home X, Y, Z
  HELP                  = print commands and settings

  Relay:
  - Relay turns ON whenever E/extruder motor is active.
  - Relay turns OFF after E/extruder movement finishes.

  Rules:
  - X/Y are absolute positions.
  - Z is absolute position and must be sent separately.
  - E is exact relative wire amount.
  - EF with X/Y means continuous wire-feed speed until X/Y target is reached.
  - Do not mix Z with X/Y/E/EF.
  - Do not use E and EF together with X/Y; choose exact E amount OR continuous EF speed.
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

// Relay pin
const int relayPin = A3;

// Change this to false if your relay module is active LOW
const bool RELAY_ACTIVE_HIGH = true;

// Driver setting
const float DRIVER_STEPS_PER_REV = 800.0;

// Mechanical movement per 1 motor revolution
const float X_MM_PER_REV = 40.0;
const float Y_MM_PER_REV = 40.0;
const float Z_MM_PER_REV = 8.0;
const float E_MM_PER_REV = 40.0;  // change this to your feeder roller travel per rev

// Pulse timing
const unsigned int PULSE_WIDTH_US = 5;
const unsigned int MIN_STEP_INTERVAL_US = 50;

// Default speeds in mm/min
float xyFeedrateMMmin = 600.0;
float zFeedrateMMmin  = 200.0;
float eFeedrateMMmin  = 200.0;

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

void relayOn() {
  digitalWrite(relayPin, RELAY_ACTIVE_HIGH ? HIGH : LOW);
}

void relayOff() {
  digitalWrite(relayPin, RELAY_ACTIVE_HIGH ? LOW : HIGH);
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

  pinMode(relayPin, OUTPUT);
  relayOff();

  digitalWrite(extrudersteppin, LOW);
  digitalWrite(xsteppin, LOW);
  digitalWrite(ysteppin, LOW);
  digitalWrite(zsteppin, LOW);

  Serial.begin(9600);
  Serial.println(F("TIG/DED CNC control ready"));

  printHelp();
}

long mmToSteps(float mm, float mmPerRev) {
  mm = floatAbs(mm);
  return (long)((mm * DRIVER_STEPS_PER_REV / mmPerRev) + 0.5);
}

float stepsPerMM(float mmPerRev) {
  return DRIVER_STEPS_PER_REV / mmPerRev;
}

unsigned long calculateStepIntervalFromTimeUs(float moveTimeUs, long maxSteps) {
  if (moveTimeUs <= 0 || maxSteps <= 0) {
    return 800;
  }

  unsigned long intervalUs = (unsigned long)(moveTimeUs / maxSteps);

  if (intervalUs < MIN_STEP_INTERVAL_US) {
    intervalUs = MIN_STEP_INTERVAL_US;
  }

  return intervalUs;
}

float calculateMoveTimeSeconds(float distanceMM, float speedMMmin) {
  if (distanceMM <= 0 || speedMMmin <= 0) return 0.0;
  return (distanceMM / speedMMmin) * 60.0;
}

void printHelp() {
  Serial.println();
  Serial.println(F("=== TIG/DED CNC Commands ==="));
  Serial.println(F("X100                  = go to X position 100 mm"));
  Serial.println(F("Y100                  = go to Y position 100 mm"));
  Serial.println(F("X100 Y0 F600          = move X/Y at 600 mm/min"));
  Serial.println(F("E10                   = extrude exact 10 mm"));
  Serial.println(F("E10 EF200             = extrude exact 10 mm at 200 mm/min"));
  Serial.println(F("X100 Y0 E10 F600      = move X/Y and extrude exact 10 mm during move"));
  Serial.println(F("X100 Y0 EF200 F600    = move X/Y and feed wire at 200 mm/min until target"));
  Serial.println(F("Z50 F200              = move Z to 50 mm at 200 mm/min"));
  Serial.println(F("XYF600                = set default X/Y speed"));
  Serial.println(F("ZF200                 = set default Z speed"));
  Serial.println(F("EF200                 = set default wire-feed speed"));
  Serial.println(F("HOME                  = home X, Y, Z"));
  Serial.println(F("HELP                  = print this menu"));
  Serial.println();

  Serial.println(F("Relay:"));
  Serial.println(F("Relay ON during E/extruder movement."));
  Serial.println(F("Relay OFF when E/extruder movement finishes."));
  Serial.println();

  Serial.println(F("Current speeds:"));
  Serial.print(F("XY speed = "));
  Serial.print(xyFeedrateMMmin);
  Serial.println(F(" mm/min"));

  Serial.print(F("Z speed  = "));
  Serial.print(zFeedrateMMmin);
  Serial.println(F(" mm/min"));

  Serial.print(F("E speed  = "));
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

void moveEExactOnly(float eMM, float eSpeedMMmin) {
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

  float eTimeSec = calculateMoveTimeSeconds(floatAbs(eMM), eFeedrateMMmin);
  float eTimeUs = eTimeSec * 1000000.0;
  unsigned long eIntervalUs = calculateStepIntervalFromTimeUs(eTimeUs, eSteps);

  Serial.print(F("E exact move: E = "));
  Serial.print(eMM);
  Serial.print(F(" mm   EF = "));
  Serial.print(eFeedrateMMmin);
  Serial.print(F(" mm/min   time = "));
  Serial.print(eTimeSec);
  Serial.print(F(" s   steps = "));
  Serial.print(eSteps);
  Serial.print(F("   interval us = "));
  Serial.println(eIntervalUs);

  relayOn();
  Serial.println(F("Relay ON"));

  for (long i = 0; i < eSteps; i++) {
    pulsePin(extrudersteppin, eIntervalUs);
  }

  relayOff();
  Serial.println(F("Relay OFF"));

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

  float zTimeSec = calculateMoveTimeSeconds(floatAbs(zMoveMM), zFeedrateMMmin);
  float zTimeUs = zTimeSec * 1000000.0;
  unsigned long zIntervalUs = calculateStepIntervalFromTimeUs(zTimeUs, zSteps);

  digitalWrite(zdirpin, zDir ? HIGH : LOW);
  delayMicroseconds(10);

  Serial.print(F("Z move to "));
  Serial.print(targetZ);
  Serial.print(F(" mm   F = "));
  Serial.print(zFeedrateMMmin);
  Serial.print(F(" mm/min   time = "));
  Serial.print(zTimeSec);
  Serial.print(F(" s   steps = "));
  Serial.println(zSteps);

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

void moveXYWithExtrusion(
  float targetX,
  bool moveX,
  float targetY,
  bool moveY,
  bool useExactE,
  float exactEMM,
  bool useContinuousEF,
  float continuousEFMMmin,
  float xySpeedMMmin
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

  float xyDistanceMM = sqrt((xMoveMM * xMoveMM) + (yMoveMM * yMoveMM));

  if (xyDistanceMM <= 0) {
    Serial.println(F("No X/Y distance to move"));
    Serial.println(F("OK"));
    return;
  }

  xyFeedrateMMmin = xySpeedMMmin;

  float xyTimeSec = calculateMoveTimeSeconds(xyDistanceMM, xyFeedrateMMmin);
  float xyTimeMin = xyTimeSec / 60.0;

  float eMoveMM = 0.0;

  if (useExactE) {
    eMoveMM = exactEMM;
  }

  if (useContinuousEF) {
    eFeedrateMMmin = continuousEFMMmin;
    eMoveMM = eFeedrateMMmin * xyTimeMin;
  }

  long xSteps = mmToSteps(xMoveMM, X_MM_PER_REV);
  long ySteps = mmToSteps(yMoveMM, Y_MM_PER_REV);
  long eSteps = mmToSteps(eMoveMM, E_MM_PER_REV);

  long maxSteps = max(xSteps, max(ySteps, eSteps));

  if (maxSteps == 0) {
    Serial.println(F("Nothing to move"));
    Serial.println(F("OK"));
    return;
  }

  bool xDir = xMoveMM >= 0 ? X_POSITIVE_DIR : !X_POSITIVE_DIR;
  bool yDir = yMoveMM >= 0 ? Y_POSITIVE_DIR : !Y_POSITIVE_DIR;
  bool eDir = eMoveMM >= 0 ? E_POSITIVE_DIR : !E_POSITIVE_DIR;

  if (xSteps > 0) digitalWrite(xdirpin, xDir ? HIGH : LOW);
  if (ySteps > 0) digitalWrite(ydirpin, yDir ? HIGH : LOW);
  if (eSteps > 0) digitalWrite(extruderdirpin, eDir ? HIGH : LOW);

  delayMicroseconds(10);

  float totalTimeUs = xyTimeSec * 1000000.0;
  unsigned long stepIntervalUs = calculateStepIntervalFromTimeUs(totalTimeUs, maxSteps);

  Serial.print(F("XY move   Xtarget = "));
  Serial.print(moveX ? targetX : currentX);
  Serial.print(F("   Ytarget = "));
  Serial.println(moveY ? targetY : currentY);

  Serial.print(F("XY distance = "));
  Serial.print(xyDistanceMM);
  Serial.print(F(" mm   F = "));
  Serial.print(xyFeedrateMMmin);
  Serial.print(F(" mm/min   XY time = "));
  Serial.print(xyTimeSec);
  Serial.println(F(" s"));

  if (useExactE) {
    Serial.print(F("E exact amount = "));
    Serial.print(eMoveMM);
    Serial.println(F(" mm, spread across XY move"));
  }

  if (useContinuousEF) {
    Serial.print(F("EF continuous speed = "));
    Serial.print(eFeedrateMMmin);
    Serial.print(F(" mm/min   auto E amount = "));
    Serial.print(eMoveMM);
    Serial.println(F(" mm"));
  }

  Serial.print(F("X steps = "));
  Serial.print(xSteps);
  Serial.print(F("   Y steps = "));
  Serial.print(ySteps);
  Serial.print(F("   E steps = "));
  Serial.print(eSteps);
  Serial.print(F("   common interval us = "));
  Serial.println(stepIntervalUs);

  long xAcc = 0;
  long yAcc = 0;
  long eAcc = 0;

  bool xStoppedByHome = false;
  bool yStoppedByHome = false;

  bool relayNeeded = (eSteps > 0);

  if (relayNeeded) {
    relayOn();
    Serial.println(F("Relay ON"));
  }

  for (long i = 0; i < maxSteps; i++) {
    if (xMoveMM < 0 && !xStoppedByHome && homeSwitchPressed(xHomeSwitchPin)) {
      currentX = 0.0;
      xSteps = 0;
      xStoppedByHome = true;
      Serial.println(F("X home switch hit. Current X = 0 mm"));
    }

    if (yMoveMM < 0 && !yStoppedByHome && homeSwitchPressed(yHomeSwitchPin)) {
      currentY = 0.0;
      ySteps = 0;
      yStoppedByHome = true;
      Serial.println(F("Y home switch hit. Current Y = 0 mm"));
    }

    bool stepX = false;
    bool stepY = false;
    bool stepE = false;

    if (xSteps > 0) {
      xAcc += xSteps;
      if (xAcc >= maxSteps) {
        stepX = true;
        xAcc -= maxSteps;
      }
    }

    if (ySteps > 0) {
      yAcc += ySteps;
      if (yAcc >= maxSteps) {
        stepY = true;
        yAcc -= maxSteps;
      }
    }

    if (eSteps > 0) {
      eAcc += eSteps;
      if (eAcc >= maxSteps) {
        stepE = true;
        eAcc -= maxSteps;
      }
    }

    if (stepX || stepY || stepE) {
      pulseSelectedPins(stepX, stepY, stepE);
    }

    if (stepIntervalUs > PULSE_WIDTH_US) {
      delayMicroseconds(stepIntervalUs - PULSE_WIDTH_US);
    }
  }

  if (relayNeeded) {
    relayOff();
    Serial.println(F("Relay OFF"));
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

    float eValue   = getValueByPrefix(command, "E", hasE);
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

    if (hasEF && !hasX && !hasY && !hasZ && !hasE) {
      eFeedrateMMmin = efValue;
      Serial.print(F("Default wire-feed speed set to "));
      Serial.print(eFeedrateMMmin);
      Serial.println(F(" mm/min"));
      Serial.println(F("OK"));
      return;
    }

    if (hasF && !hasX && !hasY && !hasZ && !hasE && !hasEF && !hasXYF && !hasZF) {
      xyFeedrateMMmin = fValue;
      zFeedrateMMmin = fValue;

      Serial.print(F("Default travel speed for XY and Z set to "));
      Serial.print(fValue);
      Serial.println(F(" mm/min"));
      Serial.println(F("OK"));
      return;
    }

    if (!hasX && !hasY && !hasZ && !hasE && (hasXYF || hasZF)) {
      Serial.println(F("OK"));
      return;
    }

    if (hasZ && (hasX || hasY || hasE || hasEF || hasXYF)) {
      Serial.println(F("Error: Z is independent. Send Z separately without X/Y/E/EF/XYF."));
      return;
    }

    if (hasE && hasEF && (hasX || hasY)) {
      Serial.println(F("Error: With X/Y, use either E exact amount OR EF continuous speed, not both."));
      return;
    }

    if (hasZ) {
      float zSpeed = hasZF ? zfValue : (hasF ? fValue : zFeedrateMMmin);
      moveZToPosition(targetZ, zSpeed);
    }

    else if (hasE && !hasX && !hasY) {
      float eSpeed = hasEF ? efValue : eFeedrateMMmin;
      moveEExactOnly(eValue, eSpeed);
    }

    else if (hasX || hasY) {
      float xySpeed = hasXYF ? xyfValue : (hasF ? fValue : xyFeedrateMMmin);

      bool useExactE = hasE;
      bool useContinuousEF = hasEF;

      float continuousSpeed = hasEF ? efValue : eFeedrateMMmin;

      moveXYWithExtrusion(
        targetX,
        hasX,
        targetY,
        hasY,
        useExactE,
        eValue,
        useContinuousEF,
        continuousSpeed,
        xySpeed
      );
    }

    else {
      Serial.println(F("Unknown command. Type HELP."));
    }
  }
}
