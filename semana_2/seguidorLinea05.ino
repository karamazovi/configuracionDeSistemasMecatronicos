// =====================================================
// seguidorLinea05 — Seguidor + Control de Velocidad PWM
// Arduino UNO + L293D + HC-SR04 + 2 sensores IR
// =====================================================
// Hardware:
//   sensorIzq (SL) = A0     sensorDer (SR) = A1
//   motorIzq: pinEna=9(PWM), pinIn1=3, pinIn2=2
//   motorDer: pinEnb=10(PWM), pinIn3=4, pinIn4=5
//   HC-SR04: pinTrigger=6, pinEcho=7
//
// IMPORTANTE: desconectar ENA y ENB del 5V fijo
//   y conectar a pines 9 y 10 del Arduino.
//
// Novedad respecto a 03:
//   - analogWrite() en ENA/ENB para variar velocidad
//   - Corrección proporcional: más error → más diferencia
//     de velocidad entre motores → giro más suave
//   - velIzq = velBase + correccion
//   - velDer = velBase - correccion
// =====================================================

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

const float        kp         = 0.20; // ganancia proporcional
const int          velBase    = 180;  // PWM base (0-255)
const int          velMin     = 60;   // PWM mínimo — motor no para
const int          distMin    = 25;   // cm — distancia para evadir
const int          tRetroceso = 700;  // ms
const int          tGiro      = 500;  // ms
const unsigned long intervaloMs = 20; // ms — ciclo (50 Hz)

unsigned long tAnterior = 0;

// ── Movimiento con PWM ────────────────────────────

void moverMotores(int velIzq, int velDer) {
  velIzq = constrain(velIzq, velMin, 255);
  velDer = constrain(velDer, velMin, 255);
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
  long d = pulseIn(pinEcho, HIGH, 25000);
  return d == 0 ? 999.0 : d * 0.034 / 2.0;
}

// ── Evasión simple ────────────────────────────────

void evitarObstaculo() {
  Serial.println("Obstaculo! Evadiendo...");
  detener();
  retroceder(); delay(tRetroceso);
  girarDerecha(); delay(tGiro);
  detener();
}

// ── Seguimiento con velocidad proporcional ────────

void seguirLineaPWM() {
  float lecturaSl = analogRead(pinSl);
  float lecturaSr = analogRead(pinSr);

  // error positivo → SR ve más negro → ir a la izquierda
  float error      = lecturaSr - lecturaSl;
  float correccion = kp * error;

  int velIzq = velBase + (int)correccion;
  int velDer = velBase - (int)correccion;

  moverMotores(velIzq, velDer);

  Serial.print("SL:"); Serial.print((int)lecturaSl);
  Serial.print("  SR:"); Serial.print((int)lecturaSr);
  Serial.print("  Err:"); Serial.print(error, 1);
  Serial.print("  Corr:"); Serial.print(correccion, 1);
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
  Serial.begin(9600);
}

void loop() {
  unsigned long tAhora = millis();
  if (tAhora - tAnterior >= intervaloMs) {
    tAnterior = tAhora;

    float dist = medirDistancia();

    if (dist < distMin) {
      evitarObstaculo();
      return;
    }

    seguirLineaPWM();
  }
}
