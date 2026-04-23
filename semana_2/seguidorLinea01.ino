// =====================================================
// seguidorLinea01 — Seguidor de Línea con delay()
// Arduino UNO + L293D + 2 sensores IR
// =====================================================
// Hardware:
//   sensorIzq (SL) = A0
//   sensorDer (SR) = A1
//   motorIzq: pinIn1=3, pinIn2=2
//   motorDer: pinIn3=4, pinIn4=5
//   Enable L293D conectado a 5V
//
// Tabla de Verdad:
//   SL | SR | Accion
//   ---|----|------------------
//    0 |  0 | avanzar
//    0 |  1 | girarIzquierda
//    1 |  0 | girarDerecha
//    1 |  1 | detener
//
// SL/SR = 1 cuando el sensor VE la linea negra (ADC < umbral)
// delay(20) al final del loop garantiza 20 ms por ciclo (50 Hz)
// =====================================================

const int pinSl  = A0;
const int pinSr  = A1;
const int pinIn1 = 3;
const int pinIn2 = 2;
const int pinIn3 = 4;
const int pinIn4 = 5;

const int umbral   = 4;   // ADC — negro si lectura < umbral
const int cicloMs  = 20;  // ms  — duracion minima de cada accion

void avanzar() {
  digitalWrite(pinIn1, HIGH); digitalWrite(pinIn2, LOW);
  digitalWrite(pinIn3, HIGH); digitalWrite(pinIn4, LOW);
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

void setup() {
  pinMode(pinIn1, OUTPUT); pinMode(pinIn2, OUTPUT);
  pinMode(pinIn3, OUTPUT); pinMode(pinIn4, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  int valSl = analogRead(pinSl);
  int valSr = analogRead(pinSr);
  bool sl = valSl < umbral;
  bool sr = valSr < umbral;

  Serial.print("SL:"); Serial.print(valSl);
  Serial.print("("); Serial.print(sl); Serial.print(")  ");
  Serial.print("SR:"); Serial.print(valSr);
  Serial.print("("); Serial.print(sr); Serial.print(")  ");

  if      (!sl && !sr) { avanzar();        Serial.println("-> AVANZAR");         }
  else if (!sl &&  sr) { girarIzquierda(); Serial.println("-> GIRAR IZQUIERDA"); }
  else if ( sl && !sr) { girarDerecha();   Serial.println("-> GIRAR DERECHA");   }
  else                 { detener();        Serial.println("-> DETENER");          }

  delay(cicloMs);
}
