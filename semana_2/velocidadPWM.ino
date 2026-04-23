// =====================================================
// velocidadPWM — Control de Velocidad con L293D
// Arduino UNO + L293D + 2 motores DC
// =====================================================
// Hardware:
//   motorIzq: pinEna=9(PWM), pinIn1=3, pinIn2=2
//   motorDer: pinEnb=10(PWM), pinIn3=4, pinIn4=5
//
// IMPORTANTE: ENA y ENB van a pines PWM (9 y 10).
//   NO conectar a 5V fijo — deben ir al Arduino.
//
// analogWrite(pin, valor):
//   valor = 0   → motor apagado (0 V promedio)
//   valor = 128 → 50% de velocidad (~2.5 V promedio)
//   valor = 255 → velocidad máxima (5 V promedio)
//
// Este sketch recorre 4 velocidades predefinidas
// para que puedas ver y medir la diferencia.
// =====================================================

const int pinEna = 9;   // PWM — enable motor izquierdo
const int pinIn1 = 3;
const int pinIn2 = 2;
const int pinEnb = 10;  // PWM — enable motor derecho
const int pinIn3 = 4;
const int pinIn4 = 5;

// Velocidades predefinidas (0-255)
const int velApagado = 0;
const int velLenta   = 80;   // ~31% — arranca justo
const int velMedia   = 150;  // ~59%
const int velRapida  = 220;  // ~86%
const int velMaxima  = 255;  // 100%

// ── Dirección: ambos motores adelante ────────────

void adelante() {
  digitalWrite(pinIn1, HIGH); digitalWrite(pinIn2, LOW);
  digitalWrite(pinIn3, HIGH); digitalWrite(pinIn4, LOW);
}

// ── Velocidad: escribe en los pines Enable ────────

void setVelocidad(int velIzq, int velDer) {
  analogWrite(pinEna, velIzq);
  analogWrite(pinEnb, velDer);
}

// ─────────────────────────────────────────────────

void setup() {
  pinMode(pinEna, OUTPUT); pinMode(pinEnb, OUTPUT);
  pinMode(pinIn1, OUTPUT); pinMode(pinIn2, OUTPUT);
  pinMode(pinIn3, OUTPUT); pinMode(pinIn4, OUTPUT);
  Serial.begin(9600);
  adelante();  // fijar dirección — solo cambiaremos velocidad
}

void loop() {
  // ── APAGADO ──────────────────────────────────
  Serial.println("Apagado (0)");
  setVelocidad(velApagado, velApagado);
  delay(2000);

  // ── VELOCIDAD LENTA ──────────────────────────
  Serial.print("Lenta ("); Serial.print(velLenta);
  Serial.println("/255)");
  setVelocidad(velLenta, velLenta);
  delay(2000);

  // ── VELOCIDAD MEDIA ──────────────────────────
  Serial.print("Media ("); Serial.print(velMedia);
  Serial.println("/255)");
  setVelocidad(velMedia, velMedia);
  delay(2000);

  // ── VELOCIDAD RAPIDA ─────────────────────────
  Serial.print("Rapida ("); Serial.print(velRapida);
  Serial.println("/255)");
  setVelocidad(velRapida, velRapida);
  delay(2000);

  // ── VELOCIDAD MAXIMA ─────────────────────────
  Serial.print("Maxima ("); Serial.print(velMaxima);
  Serial.println("/255)");
  setVelocidad(velMaxima, velMaxima);
  delay(2000);

  // ── RAMPA: sube de 0 a 255 en pasos de 5 ────
  Serial.println("Rampa subiendo...");
  for (int v = 0; v <= 255; v += 5) {
    setVelocidad(v, v);
    Serial.print("PWM: "); Serial.println(v);
    delay(80);
  }

  // ── RAMPA: baja de 255 a 0 ───────────────────
  Serial.println("Rampa bajando...");
  for (int v = 255; v >= 0; v -= 5) {
    setVelocidad(v, v);
    Serial.print("PWM: "); Serial.println(v);
    delay(80);
  }
}
