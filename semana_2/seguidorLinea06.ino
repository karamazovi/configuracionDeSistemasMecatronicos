// =====================================================
// seguidorLinea05 — Seguidor + Radar + Control PID
// Arduino UNO + L293D + HC-SR04 + Servo + 2 sensores IR
// =====================================================
// Hardware:
//   sensorIzq (SL) = A0     sensorDer (SR) = A1
//   motorIzq: pinEna=9(PWM), pinIn1=3, pinIn2=2
//   motorDer: pinEnb=10(PWM), pinIn3=4, pinIn4=5
//   HC-SR04: pinTrigger=6, pinEcho=7
//   Servo: pinServo=11
//
// IMPORTANTE: ENA y ENB deben ir a pines PWM (9 y 10).
//   Desconectar del 5V fijo y conectar a estos pines.
//
// Novedad respecto a 04:
//   - PID diferencial: error = analogRead(SR) - analogRead(SL)
//   - analogWrite en ENA/ENB para velocidad variable por motor
//   - Anti-windup: el integrador se limita a [-limI, +limI]
//   - Serial Monitor muestra: error P I D velIzq velDer
// =====================================================

#include <Servo.h>

const int pinSl      = A0;
const int pinSr      = A1;
const int pinEna     = 9;    // PWM — enable motor izquierdo
const int pinIn1     = 3;
const int pinIn2     = 2;
const int pinEnb     = 10;   // PWM — enable motor derecho
const int pinIn3     = 4;
const int pinIn4     = 5;
const int pinTrigger = 6;
const int pinEcho    = 7;
const int pinServo   = 11;

// ── Parámetros PID (ajustar según robot) ─────────
const float kp       = 0.15;   // ganancia proporcional
const float ki       = 0.002;  // ganancia integral
const float kd       = 0.05;   // ganancia derivativa
const int   velBase  = 180;    // 0-255 — velocidad base (PWM)
const int   velMax   = 255;    // limite superior PWM
const int   velMin   = 60;     // limite inferior PWM (sin parar)
const float limI     = 200.0;  // anti-windup — limite del integrador

// ── Parámetros de tiempo y evasion ────────────────
const int          distMin     = 25;   // cm
const int          tRetroceso  = 700;  // ms
const int          tGiro       = 500;  // ms
const int          angIzq      = 0;
const int          angFrente   = 90;
const int          angDer      = 180;
const unsigned long intervaloMs = 20;  // ms (50 Hz)

Servo servoRadar;
unsigned long tAnterior  = 0;
float         errorPrev  = 0;
float         integrador = 0;

// ── Movimiento con PWM ────────────────────────────

void moverMotores(int velIzq, int velDer) {
  velIzq = constrain(velIzq, velMin, velMax);
  velDer = constrain(velDer, velMin, velMax);
  digitalWrite(pinIn1, HIGH); digitalWrite(pinIn2, LOW);
  digitalWrite(pinIn3, HIGH); digitalWrite(pinIn4, LOW);
  analogWrite(pinEna, velIzq);
  analogWrite(pinEnb, velDer);
}

void retroceder() {
  digitalWrite(pinIn1, LOW); digitalWrite(pinIn2, HIGH);
  digitalWrite(pinIn3, LOW); digitalWrite(pinIn4, HIGH);
  analogWrite(pinEna, velBase);
  analogWrite(pinEnb, velBase);
}

void girarDerecha() {
  digitalWrite(pinIn1, HIGH); digitalWrite(pinIn2, LOW);
  digitalWrite(pinIn3, LOW);  digitalWrite(pinIn4, LOW);
  analogWrite(pinEna, velBase);
  analogWrite(pinEnb, 0);
}

void girarIzquierda() {
  digitalWrite(pinIn1, LOW);  digitalWrite(pinIn2, LOW);
  digitalWrite(pinIn3, HIGH); digitalWrite(pinIn4, LOW);
  analogWrite(pinEna, 0);
  analogWrite(pinEnb, velBase);
}

void detener() {
  analogWrite(pinEna, 0);
  analogWrite(pinEnb, 0);
}

// ── Ultrasonido ───────────────────────────────────

float medirDistancia() {
  digitalWrite(pinTrigger, LOW);  delayMicroseconds(2);
  digitalWrite(pinTrigger, HIGH); delayMicroseconds(10);
  digitalWrite(pinTrigger, LOW);
  long duracion = pulseIn(pinEcho, HIGH, 25000);
  return duracion == 0 ? 999.0 : duracion * 0.034 / 2.0;
}

// ── Radar ─────────────────────────────────────────

void escanearRadar(float &distIzq, float &distDer) {
  servoRadar.write(angIzq); delay(300); distIzq = medirDistancia();
  servoRadar.write(angDer); delay(300); distDer = medirDistancia();
  servoRadar.write(angFrente); delay(200);
}

// ── Evasión inteligente ───────────────────────────

void evitarObstaculo() {
  detener();
  integrador = 0; errorPrev = 0;  // resetear PID al evadir
  float distIzq, distDer;
  escanearRadar(distIzq, distDer);
  retroceder(); delay(tRetroceso); detener();
  if (distIzq > distDer) { girarIzquierda(); }
  else                    { girarDerecha();   }
  delay(tGiro);
  detener();
}

// ── Control PID diferencial ───────────────────────

void controlPID() {
  float lecturaSl = analogRead(pinSl);
  float lecturaSr = analogRead(pinSr);

  // error positivo → SR ve mas negro → girar izquierda (reducir velDer)
  float error = lecturaSr - lecturaSl;

  integrador += error;
  integrador  = constrain(integrador, -limI, limI);  // anti-windup

  float derivada  = error - errorPrev;
  errorPrev       = error;

  float correccion = kp * error + ki * integrador + kd * derivada;

  int velIzq = velBase + (int)correccion;
  int velDer = velBase - (int)correccion;

  moverMotores(velIzq, velDer);

  Serial.print("Err:"); Serial.print(error, 1);
  Serial.print("  P:"); Serial.print(kp * error, 1);
  Serial.print("  I:"); Serial.print(ki * integrador, 1);
  Serial.print("  D:"); Serial.print(kd * derivada, 1);
  Serial.print("  vIzq:"); Serial.print(velIzq);
  Serial.print("  vDer:"); Serial.println(velDer);
}

// ─────────────────────────────────────────────────

void setup() {
  pinMode(pinEna, OUTPUT); pinMode(pinEnb, OUTPUT);
  pinMode(pinIn1, OUTPUT); pinMode(pinIn2, OUTPUT);
  pinMode(pinIn3, OUTPUT); pinMode(pinIn4, OUTPUT);
  pinMode(pinTrigger, OUTPUT);
  pinMode(pinEcho, INPUT);
  servoRadar.attach(pinServo);
  servoRadar.write(angFrente);
  delay(500);
  Serial.begin(9600);
}

void loop() {
  unsigned long tAhora = millis();
  if (tAhora - tAnterior >= intervaloMs) {
    tAnterior = tAhora;

    servoRadar.write(angFrente);
    float distancia = medirDistancia();

    if (distancia < distMin) {
      evitarObstaculo();
      return;
    }

    controlPID();
  }
}
