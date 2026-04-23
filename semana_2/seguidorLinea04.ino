// =====================================================
// seguidorLinea04 — Seguidor + Radar Servo Inteligente
// Arduino UNO + L293D + HC-SR04 + Servo + 2 sensores IR
// =====================================================
// Hardware:
//   sensorIzq (SL) = A0     sensorDer (SR) = A1
//   motorIzq: pinIn1=3, pinIn2=2
//   motorDer: pinIn3=4, pinIn4=5
//   HC-SR04: pinTrigger=6, pinEcho=7
//   Servo: pinServo=11
//   Enable L293D conectado a 5V
//
// Novedad respecto a 03:
//   - Agrega servo que gira el HC-SR04 (radar)
//   - Al detectar obstaculo escanea izq y der
//   - Gira hacia el lado con mas espacio libre
// =====================================================

#include <Servo.h>

const int pinSl      = A0;
const int pinSr      = A1;
const int pinIn1     = 3;
const int pinIn2     = 2;
const int pinIn3     = 4;
const int pinIn4     = 5;
const int pinTrigger = 6;
const int pinEcho    = 7;
const int pinServo   = 11;

const int          umbral      = 4;    // ADC — negro si lectura < umbral
const int          distMin     = 25;   // cm  — distancia minima para evadir
const int          tRetroceso  = 700;  // ms
const int          tGiro       = 500;  // ms
const int          angIzq      = 0;    // grados — radar izquierda
const int          angFrente   = 90;   // grados — radar frente
const int          angDer      = 180;  // grados — radar derecha
const unsigned long intervaloMs = 20;  // ms  — periodo de muestreo (50 Hz)

Servo servoRadar;
unsigned long tAnterior = 0;

// ── Movimiento ────────────────────────────────────

void avanzar() {
  digitalWrite(pinIn1, HIGH); digitalWrite(pinIn2, LOW);
  digitalWrite(pinIn3, HIGH); digitalWrite(pinIn4, LOW);
}

void retroceder() {
  digitalWrite(pinIn1, LOW); digitalWrite(pinIn2, HIGH);
  digitalWrite(pinIn3, LOW); digitalWrite(pinIn4, HIGH);
}

void girarDerecha() {
  digitalWrite(pinIn1, HIGH); digitalWrite(pinIn2, LOW);
  digitalWrite(pinIn3, LOW);  digitalWrite(pinIn4, LOW);
}

void girarIzquierda() {
  digitalWrite(pinIn1, LOW);  digitalWrite(pinIn2, LOW);
  digitalWrite(pinIn3, HIGH); digitalWrite(pinIn4, LOW);
}

void detener() {
  digitalWrite(pinIn1, LOW); digitalWrite(pinIn2, LOW);
  digitalWrite(pinIn3, LOW); digitalWrite(pinIn4, LOW);
}

// ── Ultrasonido ───────────────────────────────────

float medirDistancia() {
  digitalWrite(pinTrigger, LOW);  delayMicroseconds(2);
  digitalWrite(pinTrigger, HIGH); delayMicroseconds(10);
  digitalWrite(pinTrigger, LOW);
  long duracion = pulseIn(pinEcho, HIGH, 25000);
  return duracion == 0 ? 999.0 : duracion * 0.034 / 2.0;
}

// ── Radar: escanea izq y der ──────────────────────

void escanearRadar(float &distIzq, float &distDer) {
  servoRadar.write(angIzq); delay(300); distIzq = medirDistancia();
  servoRadar.write(angDer); delay(300); distDer = medirDistancia();
  servoRadar.write(angFrente); delay(200);
  Serial.print("Radar Izq:"); Serial.print(distIzq);
  Serial.print("  Der:"); Serial.println(distDer);
}

// ── Evasión inteligente ───────────────────────────

void evitarObstaculo() {
  Serial.println("Obstaculo! Escaneando...");
  detener();
  float distIzq, distDer;
  escanearRadar(distIzq, distDer);
  retroceder(); delay(tRetroceso); detener();
  if (distIzq > distDer) {
    girarIzquierda();
    Serial.println("-> Girando izquierda (mas espacio)");
  } else {
    girarDerecha();
    Serial.println("-> Girando derecha (mas espacio)");
  }
  delay(tGiro);
  detener();
}

// ── Seguimiento de línea ──────────────────────────

void seguirLinea() {
  int  valSl = analogRead(pinSl);
  int  valSr = analogRead(pinSr);
  bool sl    = valSl < umbral;
  bool sr    = valSr < umbral;

  Serial.print("SL:"); Serial.print(valSl);
  Serial.print("("); Serial.print(sl); Serial.print(")  ");
  Serial.print("SR:"); Serial.print(valSr);
  Serial.print("("); Serial.print(sr); Serial.print(")  ");

  if      (!sl && !sr) { avanzar();        Serial.println("-> AVANZAR");         }
  else if (!sl &&  sr) { girarIzquierda(); Serial.println("-> GIRAR IZQUIERDA"); }
  else if ( sl && !sr) { girarDerecha();   Serial.println("-> GIRAR DERECHA");   }
  else                 { detener();        Serial.println("-> DETENER");          }
}

// ─────────────────────────────────────────────────

void setup() {
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
    Serial.print("Dist: "); Serial.print(distancia); Serial.print("cm  ");

    if (distancia < distMin) {
      evitarObstaculo();
      return;
    }

    seguirLinea();
  }
}
