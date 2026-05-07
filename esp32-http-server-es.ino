/* ============================================================
   ACTIVIDAD 1 — Servidor HTTP ESP32: Control de LEDs
   Plataforma : ESP32 DevKit C v4
   Simulador  : Wokwi (abrir http://localhost:8180)
   LED 1      : GPIO 26
   LED 2      : GPIO 27
   ============================================================ */

// ── LIBRERÍAS ─────────────────────────────────────────────────────────────────

#include <WiFi.h>           // Permite al ESP32 conectarse a una red WiFi
#include <WiFiClient.h>     // Define la clase cliente TCP que WebServer usa internamente
#include <WebServer.h>      // Proporciona la clase WebServer para crear un servidor HTTP
#include <uri/UriBraces.h>  // Extiende WebServer para soportar rutas con {parámetros}

// ── CREDENCIALES WIFI ─────────────────────────────────────────────────────────

#define WIFI_SSID     "Wokwi-GUEST"  // Nombre de la red WiFi del simulador Wokwi
#define WIFI_PASSWORD ""             // Contraseña vacía: la red Wokwi-GUEST es abierta
#define WIFI_CHANNEL  6              // Canal WiFi fijo: evita escanear todos los canales y conecta más rápido

// ── OBJETO SERVIDOR ───────────────────────────────────────────────────────────

WebServer servidor(80);  // Crea un servidor HTTP que escucha en el puerto 80 (puerto HTTP estándar)

// ── PINES DE LOS LEDs ─────────────────────────────────────────────────────────

const int LED1 = 26;  // El LED 1 está conectado físicamente al GPIO 26 del ESP32
const int LED2 = 27;  // El LED 2 está conectado físicamente al GPIO 27 del ESP32

// ── ESTADO DE LOS LEDs ────────────────────────────────────────────────────────

bool estadoLed1 = false;  // Variable que guarda si el LED 1 está encendido (true) o apagado (false)
bool estadoLed2 = false;  // Variable que guarda si el LED 2 está encendido (true) o apagado (false)

// ── FUNCIÓN: CONSTRUIR Y ENVIAR LA PÁGINA HTML ────────────────────────────────

void enviarHtml() {                    // Declara la función que genera y envía la respuesta HTTP

  String respuesta = R"(              // R"(...)" es un raw string literal: permite escribir HTML con comillas sin escaparlas
    <!DOCTYPE html><html>             <!--Declaración del tipo de documento HTML5-->
      <head>                          <!--Inicio de la cabecera: metadatos y estilos-->
        <title>Demo Servidor Web ESP32</title>                          <!--Título que aparece en la pestaña del navegador-->
        <meta name="viewport" content="width=device-width, initial-scale=1">  <!--Hace la página adaptable a pantallas móviles-->
        <style>                       <!--Inicio del bloque de estilos CSS-->
          html { font-family: sans-serif; text-align: center; }         <!--Fuente sin serifa y texto centrado en toda la página-->
          body { display: inline-flex; flex-direction: column; }        <!--Organiza los elementos en columna centrada-->
          h1 { margin-bottom: 1.2em; }                                  <!--Separa el título del resto del contenido-->
          h2 { margin: 0; }                                             <!--Elimina el margen de los subtítulos-->
          div { display: grid; grid-template-columns: 1fr 1fr;          <!--Grid de 2 columnas iguales para los botones-->
                grid-template-rows: auto auto; grid-auto-flow: column; grid-gap: 1em; }  <!--Flujo en columna con separación-->
          .btn { background-color: #5B5; border: none; color: #fff;     <!--Botón verde, sin borde, texto blanco-->
                 padding: 0.5em 1em; font-size: 2em; text-decoration: none }  <!--Relleno, tamaño grande, sin subrayado-->
          .btn.OFF { background-color: #333; }                          <!--Cuando el LED está apagado el botón se vuelve gris oscuro-->
        </style>                      <!--Fin del bloque CSS-->
      </head>                         <!--Fin de la cabecera-->

      <body>                          <!--Inicio del cuerpo visible de la página-->
        <h1>ESP32 Web Server</h1>     <!--Título principal de la página-->

        <div>                         <!--Contenedor grid que organiza los 4 elementos en 2 columnas-->
          <h2>LED 1</h2>              <!--Etiqueta del primer LED-->
          <!-- TEXTO_LED1 se reemplaza en C++ con "ON" u "OFF" antes de enviar -->
          <a href="/toggle/1" class="btn TEXTO_LED1">TEXTO_LED1</a>  <!--Enlace que al hacer clic pide /toggle/1 al ESP32-->
          <h2>LED 2</h2>              <!--Etiqueta del segundo LED-->
          <a href="/toggle/2" class="btn TEXTO_LED2">TEXTO_LED2</a>  <!--Enlace que al hacer clic pide /toggle/2 al ESP32-->
        </div>                        <!--Fin del contenedor grid-->
      </body>                         <!--Fin del cuerpo-->
    </html>                           <!--Fin del documento HTML-->
  )";                                 // Cierre del raw string literal en C++

  // replace busca el marcador TEXTO_LED1 en el String y lo sustituye:
  // si estadoLed1 es true → coloca "ON", si es false → coloca "OFF"
  // El marcador aparece dos veces (en class= y como texto) → ambas se reemplazan
  respuesta.replace("TEXTO_LED1", estadoLed1 ? "ON" : "OFF");  // Reemplaza estado del LED 1 en el HTML
  respuesta.replace("TEXTO_LED2", estadoLed2 ? "ON" : "OFF");  // Reemplaza estado del LED 2 en el HTML

  servidor.send(200, "text/html", respuesta);  // Envía al navegador: código 200 (OK), tipo MIME HTML, y el contenido
}                                              // Fin de la función enviarHtml

// ── setup(): SE EJECUTA UNA SOLA VEZ AL ENCENDER EL ESP32 ────────────────────

void setup(void) {                         // Arduino llama setup() automáticamente al arrancar

  Serial.begin(115200);                    // Abre el puerto serie a 115200 baudios para ver mensajes en el monitor
  pinMode(LED1, OUTPUT);                   // Configura el GPIO 26 como salida digital (puede encenderse/apagarse)
  pinMode(LED2, OUTPUT);                   // Configura el GPIO 27 como salida digital

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);  // Inicia el proceso de conexión a la red WiFi
  Serial.print("Conectando a WiFi ");                  // Imprime mensaje de inicio de conexión
  Serial.print(WIFI_SSID);                             // Imprime el nombre de la red para confirmación

  while (WiFi.status() != WL_CONNECTED) {  // Repite el bucle mientras NO haya conexión establecida
    delay(100);                            // Espera 100 milisegundos antes de volver a verificar
    Serial.print(".");                     // Imprime un punto por cada intento (indicador visual de progreso)
  }                                        // Cuando WiFi.status() == WL_CONNECTED el bucle termina

  Serial.println(" ¡Conectado!");          // Informa que la conexión WiFi fue exitosa

  Serial.print("Dirección IP: ");          // Etiqueta para la IP que se imprimirá a continuación
  Serial.println(WiFi.localIP());          // Imprime la dirección IP asignada al ESP32 por el router (o Wokwi)

  servidor.on("/", enviarHtml);            // Asocia la ruta "/" con la función enviarHtml: cuando el navegador pide la raíz, la llama

  servidor.on(UriBraces("/toggle/{}"), []() {   // Asocia la ruta /toggle/{n} con una función anónima (lambda)
    String led = servidor.pathArg(0);           // Extrae el texto capturado en {} — será "1" o "2"
    Serial.print("Cambiar LED #");              // Imprime etiqueta en el monitor serie
    Serial.println(led);                        // Imprime el número del LED que se va a cambiar

    switch (led.toInt()) {                      // Convierte el String a entero y evalúa cuál LED cambiar
      case 1:                                   // Si el número es 1 → actuar sobre LED 1
        estadoLed1 = !estadoLed1;              // Invierte el estado: true→false o false→true (toggle)
        digitalWrite(LED1, estadoLed1);        // Aplica el nuevo estado al pin físico GPIO 26
        break;                                 // Sale del switch para no ejecutar el case 2
      case 2:                                  // Si el número es 2 → actuar sobre LED 2
        estadoLed2 = !estadoLed2;             // Invierte el estado del LED 2
        digitalWrite(LED2, estadoLed2);       // Aplica el nuevo estado al pin físico GPIO 27
        break;                                // Sale del switch
    }                                         // Fin del switch

    enviarHtml();                             // Responde al navegador con la página actualizada (nueva clase ON/OFF)
  });                                         // Fin de la función lambda y del servidor.on

  servidor.begin();                           // Arranca el servidor HTTP: ya puede recibir peticiones
  Serial.println("Servidor HTTP iniciado (http://localhost:8180)");  // Confirma en el monitor que el servidor está activo
}                                             // Fin de setup()

// ── loop(): SE EJECUTA CONTINUAMENTE DESPUÉS DE setup() ──────────────────────

void loop(void) {                    // Arduino llama loop() repetidamente en un ciclo infinito
  servidor.handleClient();           // Revisa si llegó alguna petición HTTP y la despacha al handler registrado
  delay(2);                          // Pausa de 2 ms para dar tiempo al sistema operativo del ESP32 (RTOS)
}                                    // Fin de loop() — Arduino vuelve a llamarla inmediatamente
