/* ============================================================
   ACTIVIDAD 2 — Servidor HTTP ESP32: Control de LEDs + Motor a Pasos
   Plataforma : ESP32 DevKit C v4
   Simulador  : Wokwi (abrir http://localhost:8180)
   LED 1      : GPIO 26     LED 2     : GPIO 27
   Motor A+   : GPIO 5      Motor B+  : GPIO 17
   Motor A-   : GPIO 18     Motor B-  : GPIO 16
   ============================================================ */

// ── LIBRERÍAS ─────────────────────────────────────────────────────────────────

#include <WiFi.h>           // Permite al ESP32 conectarse a una red WiFi
#include <WiFiClient.h>     // Define la clase cliente TCP que WebServer usa internamente
#include <WebServer.h>      // Proporciona la clase WebServer para crear un servidor HTTP
#include <uri/UriBraces.h>  // Extiende WebServer para soportar rutas con {parámetros}
// Nota: NO se usa Stepper.h porque no invierte correctamente en ESP32
// El control de dirección se implementa manualmente con la secuencia de bobinas

// ── CREDENCIALES WIFI ─────────────────────────────────────────────────────────

#define WIFI_SSID     "Wokwi-GUEST"  // Nombre de la red WiFi del simulador Wokwi
#define WIFI_PASSWORD ""             // Sin contraseña: la red Wokwi-GUEST es abierta
#define WIFI_CHANNEL  6              // Canal WiFi fijo para conectar más rápido sin escanear

// ── CONSTANTES DEL MOTOR ──────────────────────────────────────────────────────

#define PASOS_POR_VUELTA 200  // Un NEMA 17 da 200 pasos para completar una vuelta (1.8° por paso)

// Array con los 4 pines del motor en orden A+, B+, A-, B- (según diagram.json)
const int MOTOR_PINS[4] = {5, 17, 18, 16};  // GPIO 5=A+, 17=B+, 18=A-, 16=B-

// Tabla de secuencia full-step para motor bipolar de 4 bobinas
// Cada fila es una fase: define qué pines van HIGH(1) y cuáles LOW(0)
// Recorrer de fase 0→1→2→3→0 = giro horario (CW)
// Recorrer de fase 0→3→2→1→0 = giro antihorario (CCW)
const int SECUENCIA[4][4] = {
  {1, 0, 0, 1},  // Fase 0: A+ activo, B- activo  → par de bobinas A+/B-
  {1, 1, 0, 0},  // Fase 1: A+ activo, B+ activo  → par de bobinas A+/B+
  {0, 1, 1, 0},  // Fase 2: B+ activo, A- activo  → par de bobinas B+/A-
  {0, 0, 1, 1}   // Fase 3: A- activo, B- activo  → par de bobinas A-/B-
};

int faseActual = 0;  // Guarda en qué fase quedó el motor tras el último movimiento

// ── OBJETO SERVIDOR ───────────────────────────────────────────────────────────

WebServer servidor(80);  // Crea el servidor HTTP en el puerto 80 (HTTP estándar)

// ── PINES DE LOS LEDs ─────────────────────────────────────────────────────────

const int LED1 = 26;  // LED 1 conectado al GPIO 26 del ESP32
const int LED2 = 27;  // LED 2 conectado al GPIO 27 del ESP32

// ── ESTADO GLOBAL ─────────────────────────────────────────────────────────────

bool estadoLed1    = false;  // true = LED 1 encendido, false = apagado
bool estadoLed2    = false;  // true = LED 2 encendido, false = apagado
int  velocidadRPM  = 10;    // Velocidad del motor en revoluciones por minuto (RPM)
int  pasosPorClick = 50;    // Cuántos pasos da el motor cada vez que se presiona CW o CCW

// ── FUNCIÓN: AVANZAR UN SOLO PASO EN LA DIRECCIÓN INDICADA ───────────────────

void darPaso(int dir) {                               // Recibe dir = +1 (CW) o -1 (CCW)
  faseActual = (faseActual + dir + 4) % 4;           // Calcula la siguiente fase; +4 evita módulo negativo en C++
  for (int i = 0; i < 4; i++) {                      // Recorre los 4 pines del motor uno por uno
    digitalWrite(MOTOR_PINS[i], SECUENCIA[faseActual][i]);  // Escribe HIGH o LOW según la tabla de secuencia
  }                                                  // Fin del bucle de pines
}                                                    // Fin de darPaso

// ── FUNCIÓN: MOVER N PASOS CON RETARDO SEGÚN RPM ─────────────────────────────

void moverMotor(int pasos) {                                         // Recibe pasos: positivo=CW, negativo=CCW
  int dir = (pasos > 0) ? 1 : -1;                                   // Determina dirección: +1 si positivo, -1 si negativo
  long delayUs = 60000000L / ((long)velocidadRPM * PASOS_POR_VUELTA); // Microsegundos entre pasos: 60s÷(RPM×200)
  for (int i = 0; i < abs(pasos); i++) {                            // Repite tantas veces como pasos (en valor absoluto)
    darPaso(dir);                                                    // Activa las bobinas para avanzar un paso
    delayMicroseconds(delayUs);                                      // Espera el tiempo calculado para la velocidad deseada
  }                                                                  // Fin del bucle de pasos
  for (int i = 0; i < 4; i++) digitalWrite(MOTOR_PINS[i], LOW);     // Apaga todas las bobinas al terminar para evitar calentamiento
}                                                                    // Fin de moverMotor

// ── HELPER: CONVIERTE int A String ───────────────────────────────────────────

inline String str(int n) { return String(n); }  // Convierte un entero a String para poder usarlo en html.replace()

// ── FUNCIÓN: CONSTRUIR Y ENVIAR LA PÁGINA HTML ────────────────────────────────

void enviarHtml() {                   // Genera el HTML con el estado actual y lo envía al navegador

  String html = R"rawhtml(            // Inicia el raw string literal: el HTML se escribe sin escapar comillas
<!DOCTYPE html><html lang="es">       <!--Documento HTML5 en español-->
<head>                                <!--Cabecera: metadatos y estilos-->
  <meta charset="UTF-8">             <!--Codificación UTF-8 para soportar tildes y caracteres especiales-->
  <meta name="viewport" content="width=device-width, initial-scale=1">  <!--Página responsiva en móviles-->
  <title>ESP32 — LEDs + Motor</title> <!--Título en la pestaña del navegador-->
  <style>                             <!--Inicio del bloque de estilos CSS-->
    * { box-sizing: border-box; margin: 0; padding: 0; }               <!--Resetea márgenes y rellenos de todos los elementos-->
    body {                            <!--Estilos del cuerpo de la página-->
      font-family: sans-serif; background: #0d1117; color: #e6edf3;    <!--Fuente, fondo oscuro y texto claro-->
      display: flex; flex-direction: column; align-items: center;       <!--Columna centrada-->
      padding: 1.5em; gap: 1.2em;    <!--Relleno exterior y separación entre secciones-->
    }
    h1 { font-size: 1.4em; color: #58a6ff; }   <!--Título principal en azul-->
    h2 { font-size: 1em; color: #8b949e; margin-bottom: 0.5em; }       <!--Subtítulo en gris-->
    section {                         <!--Estilo de las tarjetas de sección-->
      background: #161b22; border: 1px solid #30363d;                  <!--Fondo oscuro con borde sutil-->
      border-radius: 12px; padding: 1em 1.4em; width: 100%; max-width: 480px;  <!--Bordes redondeados y ancho máximo-->
    }
    .grid2 { display: grid; grid-template-columns: 1fr 1fr; gap: 0.6em; }      <!--Cuadrícula de 2 columnas iguales-->
    .grid3 { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 0.6em; }  <!--Cuadrícula de 3 columnas iguales-->
    a.btn {                           <!--Estilo base de los botones (son enlaces <a>)-->
      display: block; text-align: center; text-decoration: none;        <!--Bloque, centrado, sin subrayado-->
      padding: 0.55em 0; border-radius: 8px; font-size: 1.05em; font-weight: 600;  <!--Relleno, bordes, tamaño y negrita-->
      color: #fff; background: #238636;   <!--Texto blanco sobre fondo verde (estado ON)-->
    }
    a.btn.OFF  { background: #21262d; border: 1px solid #30363d; color: #8b949e; }  <!--Botón gris cuando el LED está apagado-->
    a.btn.cw   { background: #1f6feb; }   <!--Botón azul para girar en sentido horario (CW)-->
    a.btn.ccw  { background: #6e40c9; }   <!--Botón morado para girar en sentido antihorario (CCW)-->
    a.btn.set  { background: #b08800; font-size: 0.88em; }  <!--Botón ámbar para ajustes de velocidad y pasos-->
    .info { font-size: 0.8em; color: #8b949e; margin-top: 0.5em; text-align: center; }  <!--Texto informativo pequeño y gris-->
    .badge {                          <!--Estilo de las etiquetas de información (GPIO, NEMA 17)-->
      display: inline-block; background: #0d1117; border: 1px solid #30363d;  <!--Fondo oscuro con borde-->
      border-radius: 20px; padding: 0.1em 0.6em; font-size: 0.75em;           <!--Pastilla redondeada pequeña-->
      color: #58a6ff; font-family: monospace; margin-left: 0.4em;             <!--Texto azul en fuente monoespaciada-->
    }
  </style>                            <!--Fin del bloque CSS-->
</head>                               <!--Fin de la cabecera-->
<body>                                <!--Inicio del cuerpo visible-->
  <h1>ESP32 — LEDs + Motor a Pasos</h1>  <!--Título principal de la página-->

  <!-- ── SECCIÓN LEDs ───────────────────────────────────────── -->
  <section>                           <!--Tarjeta de control de LEDs-->
    <h2>💡 Control de LEDs</h2>       <!--Subtítulo de la sección-->
    <div class="grid2">               <!--Cuadrícula de 2 columnas: una por LED-->
      <div>                           <!--Columna del LED 1-->
        <p style="font-size:0.8em;color:#8b949e;margin-bottom:0.3em;">LED 1 <span class="badge">GPIO 26</span></p>  <!--Etiqueta con pin-->
        <a href="/toggle/1" class="btn %CLS_LED1%">%TXT_LED1%</a>  <!--Botón LED 1: clase y texto se reemplazan en C++-->
      </div>                          <!--Fin columna LED 1-->
      <div>                           <!--Columna del LED 2-->
        <p style="font-size:0.8em;color:#8b949e;margin-bottom:0.3em;">LED 2 <span class="badge">GPIO 27</span></p>  <!--Etiqueta con pin-->
        <a href="/toggle/2" class="btn %CLS_LED2%">%TXT_LED2%</a>  <!--Botón LED 2: clase y texto se reemplazan en C++-->
      </div>                          <!--Fin columna LED 2-->
    </div>                            <!--Fin grid de LEDs-->
  </section>                          <!--Fin tarjeta LEDs-->

  <!-- ── SECCIÓN MOTOR ──────────────────────────────────────── -->
  <section>                           <!--Tarjeta de control del motor a pasos-->
    <h2>⚙️ Motor a Pasos <span class="badge">NEMA 17</span></h2>  <!--Subtítulo con etiqueta del modelo-->
    <div class="grid2" style="margin-bottom:0.6em;">   <!--Cuadrícula 2 columnas para los botones de dirección-->
      <a href="/stepper/ccw" class="btn ccw">◀ CCW</a>  <!--Botón antihorario: al clicar pide /stepper/ccw al ESP32-->
      <a href="/stepper/cw"  class="btn cw">CW ▶</a>   <!--Botón horario: al clicar pide /stepper/cw al ESP32-->
    </div>                            <!--Fin grid de dirección-->
    <p class="info">Pasos por clic: <strong>%PASOS%</strong> &nbsp;|&nbsp; Velocidad: <strong>%RPM% RPM</strong></p>  <!--Muestra configuración actual-->

    <div class="grid3" style="margin-top:0.8em;">       <!--Cuadrícula 3 columnas para botones de velocidad-->
      <a href="/stepper/speed/5"  class="btn set">5 RPM</a>   <!--Fija la velocidad a 5 RPM-->
      <a href="/stepper/speed/15" class="btn set">15 RPM</a>  <!--Fija la velocidad a 15 RPM-->
      <a href="/stepper/speed/30" class="btn set">30 RPM</a>  <!--Fija la velocidad a 30 RPM-->
    </div>                            <!--Fin grid de velocidad-->
    <p class="info" style="margin-top:0.3em;">Velocidad</p>   <!--Etiqueta del grupo de botones de velocidad-->

    <div class="grid3" style="margin-top:0.7em;">       <!--Cuadrícula 3 columnas para botones de pasos por clic-->
      <a href="/stepper/steps/25"  class="btn set">25 pasos</a>   <!--Fija pasos por clic a 25 (movimiento pequeño)-->
      <a href="/stepper/steps/100" class="btn set">100 pasos</a>  <!--Fija pasos por clic a 100 (media vuelta)-->
      <a href="/stepper/steps/200" class="btn set">1 vuelta</a>   <!--Fija pasos por clic a 200 (vuelta completa)-->
    </div>                            <!--Fin grid de pasos-->
    <p class="info" style="margin-top:0.3em;">Pasos por clic</p>  <!--Etiqueta del grupo de botones de pasos-->
  </section>                          <!--Fin tarjeta motor-->
</body>                               <!--Fin del cuerpo-->
</html>                               <!--Fin del documento HTML-->
)rawhtml";                            // Cierre del raw string literal en C++

  // Reemplazar marcadores de los LEDs antes de enviar el HTML:
  html.replace("%TXT_LED1%", estadoLed1 ? "ON"  : "OFF");  // Si LED1 encendido → "ON", si no → "OFF" (texto del botón)
  html.replace("%CLS_LED1%", estadoLed1 ? ""    : "OFF");  // Si LED1 encendido → sin clase extra, si no → clase "OFF" (color gris)
  html.replace("%TXT_LED2%", estadoLed2 ? "ON"  : "OFF");  // Mismo para el texto del LED 2
  html.replace("%CLS_LED2%", estadoLed2 ? ""    : "OFF");  // Mismo para la clase del LED 2

  // Reemplazar marcadores del motor con los valores actuales:
  html.replace("%PASOS%", str(pasosPorClick));  // Inserta el número actual de pasos por clic
  html.replace("%RPM%",   str(velocidadRPM));   // Inserta la velocidad actual en RPM

  servidor.send(200, "text/html", html);  // Envía la respuesta HTTP: 200=OK, tipo MIME, y el HTML construido
}                                         // Fin de enviarHtml

// ── setup(): SE EJECUTA UNA SOLA VEZ AL ENCENDER EL ESP32 ────────────────────

void setup() {                             // Arduino ejecuta esta función automáticamente al arrancar

  Serial.begin(115200);                    // Abre el puerto serie a 115200 baudios para el monitor serie
  pinMode(LED1, OUTPUT);                   // Configura GPIO 26 como salida digital (puede enviar HIGH/LOW)
  pinMode(LED2, OUTPUT);                   // Configura GPIO 27 como salida digital

  for (int i = 0; i < 4; i++) pinMode(MOTOR_PINS[i], OUTPUT);  // Configura los 4 pines del motor como salida digital

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);  // Inicia la conexión a la red WiFi con las credenciales definidas
  Serial.print("Conectando a WiFi " WIFI_SSID);        // Imprime el nombre de la red en el monitor serie

  while (WiFi.status() != WL_CONNECTED) {  // Repite mientras el ESP32 no haya obtenido dirección IP
    delay(100);                            // Pausa 100 ms entre verificaciones para no saturar el procesador
    Serial.print(".");                     // Imprime un punto como indicador visual de espera
  }                                        // Fin del bucle de espera WiFi

  Serial.println(" ¡Conectado!");          // Confirma que la conexión WiFi fue exitosa
  Serial.print("IP: ");                    // Etiqueta previa a la impresión de la IP
  Serial.println(WiFi.localIP());          // Imprime la dirección IP asignada al ESP32

  // ── REGISTRO DE RUTAS DEL SERVIDOR ────────────────────────────────────────

  servidor.on("/", enviarHtml);            // Ruta raíz: cuando el navegador pide "/" llama a enviarHtml

  servidor.on(UriBraces("/toggle/{}"), []() {           // Ruta /toggle/{n}: cualquier número en {} activa este handler
    int led = servidor.pathArg(0).toInt();              // Convierte el texto capturado en {} a entero (1 o 2)
    if (led == 1) { estadoLed1 = !estadoLed1; digitalWrite(LED1, estadoLed1); }  // Invierte LED 1 y aplica al GPIO
    if (led == 2) { estadoLed2 = !estadoLed2; digitalWrite(LED2, estadoLed2); }  // Invierte LED 2 y aplica al GPIO
    Serial.printf("Toggle LED %d\n", led);              // Imprime cuál LED se cambió (para depuración)
    enviarHtml();                                       // Responde con la página actualizada
  });                                                   // Fin handler toggle

  servidor.on("/stepper/cw", []() {                              // Ruta /stepper/cw: gira el motor en sentido horario
    Serial.printf("Motor CW %d pasos @ %d RPM\n", pasosPorClick, velocidadRPM);  // Log del movimiento
    moverMotor(pasosPorClick);                                   // Llama a moverMotor con pasos positivos (CW)
    enviarHtml();                                                // Responde con la página actualizada
  });                                                            // Fin handler CW

  servidor.on("/stepper/ccw", []() {                             // Ruta /stepper/ccw: gira el motor en sentido antihorario
    Serial.printf("Motor CCW %d pasos @ %d RPM\n", pasosPorClick, velocidadRPM);  // Log del movimiento
    moverMotor(-pasosPorClick);                                  // Llama a moverMotor con pasos negativos (CCW)
    enviarHtml();                                                // Responde con la página actualizada
  });                                                            // Fin handler CCW

  servidor.on(UriBraces("/stepper/speed/{}"), []() {       // Ruta /stepper/speed/{n}: cambia la velocidad en RPM
    int rpm = servidor.pathArg(0).toInt();                 // Extrae y convierte el valor de RPM de la URL
    if (rpm >= 1 && rpm <= 60) {                           // Solo acepta valores entre 1 y 60 RPM (rango seguro)
      velocidadRPM = rpm;                                  // Actualiza la variable global de velocidad
      Serial.printf("Velocidad: %d RPM\n", velocidadRPM); // Confirma el cambio en el monitor serie
    }                                                      // Fin validación RPM
    enviarHtml();                                          // Responde con la página que muestra la nueva velocidad
  });                                                      // Fin handler speed

  servidor.on(UriBraces("/stepper/steps/{}"), []() {       // Ruta /stepper/steps/{n}: cambia los pasos por clic
    int pasos = servidor.pathArg(0).toInt();               // Extrae y convierte el número de pasos de la URL
    if (pasos >= 1 && pasos <= 400) {                      // Solo acepta entre 1 y 400 pasos (máximo 2 vueltas)
      pasosPorClick = pasos;                               // Actualiza la variable global de pasos por clic
      Serial.printf("Pasos por clic: %d\n", pasosPorClick); // Confirma el cambio en el monitor serie
    }                                                      // Fin validación pasos
    enviarHtml();                                          // Responde con la página que muestra los nuevos pasos
  });                                                      // Fin handler steps

  servidor.begin();                                        // Arranca el servidor HTTP: listo para recibir peticiones
  Serial.println("Servidor HTTP iniciado → http://localhost:8180");  // Confirma que el servidor está activo
}                                                          // Fin de setup()

// ── loop(): SE EJECUTA CONTINUAMENTE DESPUÉS DE setup() ──────────────────────

void loop() {                        // Arduino llama loop() en un ciclo infinito
  servidor.handleClient();           // Revisa si llegó alguna petición HTTP y llama al handler correspondiente
  delay(2);                          // Pausa de 2 ms para dar tiempo al RTOS del ESP32 de gestionar tareas internas
}                                    // Fin de loop() — Arduino vuelve a llamarla de inmediato
