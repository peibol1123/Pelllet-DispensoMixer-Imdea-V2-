#include <AccelStepper.h>

/*
  Adaptación para BIGTREETECH Octopus Pro v1.1 usando STM32duino.

  Notas importantes:
  - Este código usa los sockets de drivers M0-M7 de la Octopus Pro.
  - Los antiguos "sleepPin" del código original se sustituyen por ENABLE.
  - En drivers tipo TMC2209 / TMC5160 / A4988 / DRV8825, ENABLE suele ser activo en LOW.
  - Comando por Serial:
      '0' -> activa Motor 1 / socket M0
      '1' -> activa Motor 2 / socket M1
      ...
      '7' -> activa Motor 8 / socket M7
      '8' o 'x' -> desactiva todos los motores
  - Esta pensado para ser compilado y subido mediante un clon St-Link V2
*/

constexpr uint8_t NUM_MOTORS = 8;
constexpr int NO_ACTIVE_MOTOR = 8;

// Parámetros originales
constexpr long shakeSteps = 50;
constexpr int strokes = 3;
constexpr long dispenseStepIncrement = 15;
constexpr int dispenseDiscretizations = 30;

constexpr float shakeAcceleration = 2500.0;
constexpr float dispenseAcceleration = 1000.0;

constexpr float shakeMaxSpeed = 1000.0;
constexpr float dispenseMaxSpeed = 300.0;

// Pulso mínimo STEP. 2 us suele ser más seguro para drivers tipo TMC/A4988/DRV8825.
constexpr unsigned int minStepPulseWidthUs = 2;

// -----------------------------------------------------------------------------
// Pines BTT Octopus Pro v1.1, (diferencia en el EN del Driver3 con respecto a V1: PA0 --> PA2)
// -----------------------------------------------------------------------------

// STEP pins: M0, M1, M2, M3, M4, M5, M6, M7
const uint8_t motorStepPins[NUM_MOTORS] = {
  PF13,  // Motor 1 -> M0 STEP
  PG0,   // Motor 2 -> M1 STEP
  PF11,  // Motor 3 -> M2 STEP
  PG4,   // Motor 4 -> M3 STEP
  PF9,   // Motor 5 -> M4 STEP
  PC13,  // Motor 6 -> M5 STEP
  PE2,   // Motor 7 -> M6 STEP
  PE6    // Motor 8 -> M7 STEP
};

// DIR pins: M0, M1, M2, M3, M4, M5, M6, M7
const uint8_t motorDirPins[NUM_MOTORS] = {
  PF12,  // Motor 1 -> M0 DIR
  PG1,   // Motor 2 -> M1 DIR
  PG3,   // Motor 3 -> M2 DIR
  PC1,   // Motor 4 -> M3 DIR
  PF10,  // Motor 5 -> M4 DIR
  PF0,   // Motor 6 -> M5 DIR
  PE3,   // Motor 7 -> M6 DIR
  PA14   // Motor 8 -> M7 DIR
};

// ENABLE pins: M0, M1, M2, M3, M4, M5, M6, M7
const uint8_t motorEnablePins[NUM_MOTORS] = {
  PF14,  // Motor 1 -> M0 ENABLE
  PF15,  // Motor 2 -> M1 ENABLE
  PG5,   // Motor 3 -> M2 ENABLE
  PA2,   // Motor 4 -> M3 ENABLE. Ojo: en Octopus Pro v1.1 es PA2, en la referencia es PA0, si no funciona probar PA0
  PG2,   // Motor 5 -> M4 ENABLE
  PF1,   // Motor 6 -> M5 ENABLE
  PD4,   // Motor 7 -> M6 ENABLE
  PE0    // Motor 8 -> M7 ENABLE
};

// ENABLE activo en LOW
constexpr uint8_t DRIVER_ENABLE_STATE = LOW;
constexpr uint8_t DRIVER_DISABLE_STATE = HIGH;

// -----------------------------------------------------------------------------
// Objetos AccelStepper
// -----------------------------------------------------------------------------

AccelStepper steppers[NUM_MOTORS] = {
  AccelStepper(AccelStepper::DRIVER, motorStepPins[0], motorDirPins[0]),
  AccelStepper(AccelStepper::DRIVER, motorStepPins[1], motorDirPins[1]),
  AccelStepper(AccelStepper::DRIVER, motorStepPins[2], motorDirPins[2]),
  AccelStepper(AccelStepper::DRIVER, motorStepPins[3], motorDirPins[3]),
  AccelStepper(AccelStepper::DRIVER, motorStepPins[4], motorDirPins[4]),
  AccelStepper(AccelStepper::DRIVER, motorStepPins[5], motorDirPins[5]),
  AccelStepper(AccelStepper::DRIVER, motorStepPins[6], motorDirPins[6]),
  AccelStepper(AccelStepper::DRIVER, motorStepPins[7], motorDirPins[7])
};

int activeMotor = NO_ACTIVE_MOTOR;

// -----------------------------------------------------------------------------
// Funciones auxiliares
// -----------------------------------------------------------------------------

void setMotorEnabled(uint8_t motorIndex, bool enabled) {
  if (motorIndex >= NUM_MOTORS) {
    return;
  }

  digitalWrite(
    motorEnablePins[motorIndex],
    enabled ? DRIVER_ENABLE_STATE : DRIVER_DISABLE_STATE
  );
}

void disableAllMotors() {
  for (uint8_t i = 0; i < NUM_MOTORS; i++) {
    setMotorEnabled(i, false);
  }
}

void enableOnlyMotor(uint8_t motorIndex) {
  for (uint8_t i = 0; i < NUM_MOTORS; i++) {
    setMotorEnabled(i, i == motorIndex);
  }
}

void readSerialCommand() {
  while (Serial.available() > 0) {
    char command = (char)Serial.read();

    if (command == '\n' || command == '\r' || command == ' ') { //funcion para los saltos de linea, revisar
      continue;
    }

    // Selección de motor: '0' a '7'
    if (command >= '0' && command <= '7') {
      activeMotor = command - '0';

      enableOnlyMotor((uint8_t)activeMotor);

      Serial.print("Active motor index: ");
      Serial.println(activeMotor);
      Serial.print("Octopus socket: M");
      Serial.println(activeMotor);

      return;
    }

    // Desactivar todos
    if (command == '8' || command == 'x' || command == 'X') {
      activeMotor = NO_ACTIVE_MOTOR;
      disableAllMotors();

      Serial.println("All motors disabled.");
      return;
    }

    Serial.print("Ignored command: ");
    Serial.println(command);
  }
}

void runDispenserCycle(uint8_t motorIndex) {
  if (motorIndex >= NUM_MOTORS) {
    return;
  }

  AccelStepper &motor = steppers[motorIndex];

  // Shake
  motor.setMaxSpeed(shakeMaxSpeed);
  motor.setAcceleration(shakeAcceleration);

  // Mantengo tu lógica original: j <= strokes.
  // Con strokes = 3, esto ejecuta 4 ciclos.
  for (int j = 0; j <= strokes; j++) {
    if (!Serial.available()) {
      motor.move(shakeSteps);
      motor.runToPosition();
    } else {
      return;
    }

    if (!Serial.available()) {
      motor.move(-shakeSteps);
      motor.runToPosition();
    } else {
      return;
    }
  }

  // Dispense
  motor.setMaxSpeed(dispenseMaxSpeed);
  motor.setAcceleration(dispenseAcceleration);

  // Mantengo tu lógica original: j <= dispenseDiscretizations.
  // Con dispenseDiscretizations = 30, ejecuta 31 incrementos.
  for (int j = 0; j <= dispenseDiscretizations; j++) {
    if (!Serial.available()) {
      motor.move(dispenseStepIncrement);
      motor.runToPosition();
    } else {
      return;
    }
  }
}

// -----------------------------------------------------------------------------
// Setup / Loop
// -----------------------------------------------------------------------------

void setup() {
  Serial.begin(9600);

  // Si usas USB CDC, esto da un pequeño margen para abrir el puerto serie.
  // Si no hay USB conectado, no bloquea indefinidamente.
  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < 2000)) {
    // Espera máxima de 2 segundos
  }

  for (uint8_t i = 0; i < NUM_MOTORS; i++) {
    pinMode(motorStepPins[i], OUTPUT);
    pinMode(motorDirPins[i], OUTPUT);
    pinMode(motorEnablePins[i], OUTPUT);

    // Seguridad: todos los motores desactivados al arrancar
    digitalWrite(motorEnablePins[i], DRIVER_DISABLE_STATE);

    steppers[i].setMaxSpeed(dispenseMaxSpeed);
    steppers[i].setAcceleration(dispenseAcceleration);
    steppers[i].setMinPulseWidth(minStepPulseWidthUs);
  }

  activeMotor = NO_ACTIVE_MOTOR;

  Serial.println("BTT Octopus Pro v1.1 dispenser controller ready.");
  Serial.println("Send 0-7 to select motor M0-M7.");
  Serial.println("Send 8 or x to disable all motors.");
}

void loop() {
  readSerialCommand();

  if (activeMotor != NO_ACTIVE_MOTOR) {
    runDispenserCycle((uint8_t)activeMotor);
  }
}