// --------------------- INCLUDES ---------------------
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_AHTX0.h>
#include <DHT.h>

#include "Debug.h"


#define ENABLE_CHART 1   // ⇦ poné 0 para quitar la gráfica
#include "web_page_core.h"
#if ENABLE_CHART
  #include "web_chart.h"
#endif
#include "web_send.h"

// ===============================================================
//  CONFIGURACIÓN DE HARDWARE
// ===============================================================

// --- Pines I2C ---
#define SDA_1   21
#define SCL_1   22
#define SDA_2   18
#define SCL_2   19

// --- Pines sensores / actuadores ---
#define DHTPIN1 13
#define DHTPIN2 14
#define DHTTYPE DHT11
#define RELAY_PIN 15
#define LED       4

// ===============================================================
//  RED / SERVIDOR WEB
// ===============================================================
const char* ssid     = "electricidad";
const char* password = "medianoche extinto carreras asado hoja integral";
WiFiServer server(80);
String header;

// ===============================================================
//  TIEMPOS / PERIODOS
// ===============================================================
// Escaneo de sensores
#define SENSOR_INTERVAL_MS     1000-140   // cada 1 s
#define SENSOR_INTERVAL_MS_LOW     10000-140   // cada 1 s
#define SENSOR_INTERVAL_MS_HIGH     1000-140   // cada 1 s
// Actualización web (gráfica / fetch)
#define WEB_UPDATE_MS          500   // cada 1 s
// Timeout de cliente web
#define WEB_CLIENT_TIMEOUT_MS  2000   // 2 s sin tráfico ⇒ cortar conexión

int periodo=SENSOR_INTERVAL_MS_LOW;

unsigned long tiempoInicial = 0;  // Guarda el momento de inicio
unsigned long tiempoActual = 0;

unsigned long  lecturaAnterior;
unsigned long  tiempoLectura;

unsigned long  lecturas=0;
// ===============================================================
//  INSTANCIAS DE SENSORES
// ===============================================================
DHT dht1(DHTPIN1, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);
Adafruit_AHTX0 aht10_1;
Adafruit_AHTX0 aht10_2;
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
TwoWire I2C_2 = TwoWire(1);

// Estado de inicialización
bool mlx_ok;
bool aht1_ok;
bool aht2_ok;

// ===============================================================
//  VARIABLES DE MEDICIÓN / ESTADO
// ===============================================================
// MLX
float mlxTempObj, mlxTempAmb;

// AHT y DHT
float ahtTemp1 = 0, ahtHum1 = 0;
float ahtTemp2 = 0, ahtHum2 = 0;
float dhtTemp1, dhtHum1;
float dhtTemp2, dhtHum2;

// Límites de temperatura
float tempLimit = 0.0;
float criticalTempLimit = 0.0;
bool tempLimitConfigured = false;
bool criticalTempLimitConfigured = false;

// Relé
bool relayState  = false;
bool relayLocked = false;

// Tiempo “hh:mm:ss” (relativo)
int    tiempo         = 0;
String tiempoFormato  = "00:00:00";

// ===============================================================
//  DEBUG
// ===============================================================
Debug debug;


// Funcion para convertir float a String con formato controlado
String formatFloat(float value, int precision = 2) {
  char buffer[10] = "";
  dtostrf(value, 4, precision, buffer);
  return String(buffer);
}
//Funcion para convertir segundos a formato hh:mm:ss
String formatoTiempo(unsigned long milisegundosTotales) {
    unsigned long segundosTotales = milisegundosTotales / 1000;
    int horas = segundosTotales / 3600;
    int minutos = (segundosTotales % 3600) / 60;
    int segundos = segundosTotales % 60;
    int ms = milisegundosTotales % 1000;

    char tiempoFormato[13];
    sprintf(tiempoFormato, "%02d:%02d:%02d.%03d", horas, minutos, segundos, ms);
    //sprintf(tiempoFormato, "%02d:%02d:%02d", horas, minutos, segundos);
    return String(tiempoFormato);
}
void releUpdate(){
    if (!relayLocked && tempLimitConfigured && criticalTempLimitConfigured && relayState) {
      if (mlxTempObj >= tempLimit) {
        //relayState = false;
        digitalWrite(RELAY_PIN, HIGH);
        digitalWrite(LED, LOW);
        //debug.infof("Rele Apagado");
      } else if (mlxTempObj <= tempLimit - 5.00) {
        //relayState = true;
        digitalWrite(RELAY_PIN, LOW);
        digitalWrite(LED, HIGH);
        //debug.infof("Rele Prendido");
        //debug.infof(tempLimit);
      }
    }
}
void releUpdateDesbloqueo(){
  if (ahtTemp1 >= criticalTempLimit && criticalTempLimitConfigured) {
    relayLocked = true;
    relayState = false;
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(LED, LOW);
    //debug.infof("Limite critico alcanzado. Rele apagado definitivamente.");
  }
}
bool leerDHT(DHT &dht, float &dhtTemp, float &dhtHum, const char *ubicacion="interno") {
    bool error=false;
    float dhtTempAux = dht.readTemperature();
    float dhtHumAux = dht.readHumidity();
    if (isnan(dhtTempAux) || isnan(dhtHumAux)) { debug.errorf("❌ DHT #%s lectura inválida",ubicacion); error = true; }
    else {
            dhtTemp = dhtTempAux;
            dhtHum = dhtHumAux;
            debug.dhtf("#(%s) → T: %.2f °C | H: %.2f %%",ubicacion, dhtTemp, dhtHum);
    }
    return error; 
}
bool leerAHT(Adafruit_AHTX0 &aht10, bool aht_ok, float  &temp, float  &humidity, const char *ubicacion="interno") {
    bool error=false;
    if (aht_ok) {
    sensors_event_t humidityEvent, tempEvent;
    aht10.getEvent(&humidityEvent, &tempEvent);
    if (isnan(tempEvent.temperature) || isnan(humidityEvent.relative_humidity)) {
        debug.errorf("❌ AHT #(%s) lectura inválida ",ubicacion); error = true;
        temp = -100;
        humidity = -100;
    } else {
        debug.ahtf("AHT #(%s) → T: %.2f °C | H: %.2f %% ",ubicacion, tempEvent.temperature, humidityEvent.relative_humidity);
            temp = tempEvent.temperature;
            humidity = humidityEvent.relative_humidity;
    }
    } else { debug.errorf("⚠️ AHT #(%s) no inicializado",ubicacion); error = true; 
        temp = 0;
        humidity = 0;
    }
    return error; 
}
bool leerMLX(Adafruit_MLX90614 &mlx, bool mlx_ok, float  &mlxTempObj, float  &mlxTempAmb, const char *ubicacion="interno") {
    bool error=false;
    // Lee omfrarojo
    // ----- MLX90614 -----
    if (mlx_ok) {
    mlxTempObj = mlx.readObjectTempC();
    mlxTempAmb = mlx.readAmbientTempC();
    if (isnan(mlxTempAmb) || isnan(mlxTempObj)) { debug.errorf("❌ MLX90614 lectura inválida"); error = true; }
    else debug.mlxf("MLX90614 → Tamb: %.2f °C | Tobj: %.2f °C", mlxTempAmb, mlxTempObj);
    } else {
    debug.errorf("⚠️ MLX90614 no inicializado");
    error = true;
    }
    return error; 
}
// Funcion de tarea para el nucleo 1: leer sensores y controlar el rele
void leerSensores() {
  bool error = false;
  leerDHT(dht1, dhtTemp1, dhtHum1);
  leerDHT(dht2, dhtTemp2, dhtHum2, "externo");
  leerAHT(aht10_1,aht1_ok,ahtTemp1, ahtHum1);
  leerAHT(aht10_2,aht2_ok,ahtTemp2, ahtHum2, "externo");
  leerMLX(mlx,mlx_ok,mlxTempObj,mlxTempAmb);
  debug.infoSensor("-----------------------------");
}
void sensorTask(void *pvParameters) {
  for(;;) {
    leerSensores();
    lecturas++;
    lecturaAnterior=tiempoLectura;
    tiempoLectura=millis();
    releUpdate();
    releUpdateDesbloqueo();
    //debug.infof("tiempo de sensado: %s  Delta: %lu,  lecturas totales: %lu",tiempoFormato ,tiempoLectura-lecturaAnterior,lecturas );
    //tiempoFormato = formatoTiempo(tiempo);
    //tiempo += 1; 
    vTaskDelay(pdMS_TO_TICKS(periodo));
  }
}
// Funcion de tarea para el nucleo 2: gestionar la interfaz web
void webServerTask(void *pvParameters) {
  for (;;) {
    //Esperar conexión entrante de un cliente HTTP (navegador)
    WiFiClient client = server.available();
    if (client) {
      String currentLine = "";
      unsigned long tLast = millis();
      //Mientras el cliente siga conectado
      while (client.connected()) {
        // Ceder SIEMPRE en el bucle
        vTaskDelay(1);
        //Leer datos disponibles del cliente
        if (client.available()) {
          char c = client.read();
          header += c;
          if (c == '\n') {
            //Fin de línea → interpretar solicitud HTTP recibida
            if (currentLine.length() == 0) {
              //Solicitud a /sensordata → devuelve JSON de sensores
              if (header.indexOf("GET /sensordata") >= 0) {
                client.println("HTTP/1.1 200 OK");
                client.println("Content-Type: application/json");
                client.println("Connection: close");
                client.println();


                String jsonResponse = "{";
                jsonResponse += "\"tiempo\": \"" + tiempoFormato + "\",";
                jsonResponse += "\"tiempoLectura\": \"" + formatoTiempo(tiempoLectura) + "\",";
                jsonResponse += "\"ahtTemp1\": \"" + formatFloat(ahtTemp1) + "\",";
                jsonResponse += "\"ahtHum1\": \"" + formatFloat(ahtHum1) + "\",";
                jsonResponse += "\"ahtTemp2\": \"" + formatFloat(ahtTemp2) + "\",";
                jsonResponse += "\"ahtHum2\": \"" + formatFloat(ahtHum2) + "\",";
                jsonResponse += "\"dhtTemp1\": \"" + formatFloat(dhtTemp1) + "\",";
                jsonResponse += "\"dhtHum1\": \"" + formatFloat(dhtHum1) + "\",";
                jsonResponse += "\"dhtTemp2\": \"" + formatFloat(dhtTemp2) + "\",";
                jsonResponse += "\"dhtHum2\": \"" + formatFloat(dhtHum2) + "\",";
                jsonResponse += "\"mlxTempObj\": \"" + formatFloat(mlxTempObj) + "\",";
                jsonResponse += "\"mlxTempAmb\": \"" + formatFloat(mlxTempAmb) + "\",";
                jsonResponse += "\"relayState\": " + String(relayState ? "true" : "false") + ",";
                jsonResponse += "\"relayMessage\": \"" + String(relayLocked ? "Rele bloqueado debido a limite critico de temperatura." :
                                   (!tempLimitConfigured || !criticalTempLimitConfigured ? "Rele deshabilitado hasta que ambos limites sean configurados." :
                                   (relayState ? "Rele encendido" : "Encendido del rele habilitado."))) + "\"";
                jsonResponse += "}";
                client.print(jsonResponse);
                break;
              }
              //Solicitud a /toggleRelay → alterna el estado del relé
              if (header.indexOf("GET /toggleRelay") >= 0) {
                if (!relayLocked && tempLimitConfigured && criticalTempLimitConfigured) {
                  relayState = !relayState;
                  digitalWrite(RELAY_PIN, relayState ? LOW : HIGH);
                  digitalWrite(LED, relayState ? HIGH : LOW);
                  
                  periodo=relayState ? SENSOR_INTERVAL_MS_HIGH : SENSOR_INTERVAL_MS_LOW;
                }
                client.println("HTTP/1.1 200 OK");
                client.println("Connection: close");
                client.println();
              }
              //Configuración de límite de temperatura normal
              if (header.indexOf("GET /setlimit/?temp=") >= 0) {
                int pos = header.indexOf("GET /setlimit/?temp=") + 20;
                String tempValue = header.substring(pos, header.indexOf(" ", pos));
                if(tempValue.toFloat() >= 110.00){
                  tempLimit = 110.00;
                }
                else if(tempValue.toFloat() >= 00.00){
                  tempLimit = tempValue.toFloat();
                }
                else{
                  tempLimit = 00.00;
                }
                if (!isnan(tempLimit) && tempLimit != 0.0) {
                  tempLimitConfigured = true;
                }
              }
              // Configuración de límite crítico de temperatura
              if (header.indexOf("GET /setcriticallimit/?temp=") >= 0) {
                int pos = header.indexOf("GET /setcriticallimit/?temp=") + 28;
                String tempValue = header.substring(pos, header.indexOf(" ", pos));
                if(tempValue.toFloat() >= 90.00){
                   criticalTempLimit = 90.00;
                }
                else if (tempValue.toFloat() >= 0.00){
                  criticalTempLimit = tempValue.toFloat();
                }
                else{
                  criticalTempLimit = 00.00;
                }
                if (!isnan(criticalTempLimit) && criticalTempLimit != 0.0) {
                  criticalTempLimitConfigured = true;
                }
              }
              //Página principal "/" → devuelve el HTML desde PROGMEM
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println("Connection: close");
              client.println();

                String html = FPSTR(index_head);
                html.replace("%UPDATE_MS%", String(WEB_UPDATE_MS));
                client.print(html);

                // inyectar (o no) la gráfica
                #if ENABLE_CHART
                  sendPROGMEM(client, chart_block);
                #endif

                // cierre y scripts base
                sendPROGMEM(client, index_tail);

    
              break;
            } else {
              currentLine = "";
            }
          } else if (c != '\r') {
            currentLine += c;
          }
        }
        //Corta si pasan 2 segundos sin actividad (timeout)
        if (millis() - tLast > WEB_CLIENT_TIMEOUT_MS) break;
      }
      //Fin de conexión → limpiar encabezado y cerrar socket
      header = "";
      client.stop();
    }
    //Esperar un pequeño tiempo antes de revisar el siguiente cliente
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
void wifi_setup(){
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.printf("Conexion establecida. \n");
  Serial.printf("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}
void sensor_setup(){
  esp_log_level_set("i2c",        ESP_LOG_NONE);   // o ESP_LOG_ERROR si querés ver solo errores graves
  esp_log_level_set("i2c.master", ESP_LOG_NONE);





  Wire.begin(SDA_1, SCL_1);
  I2C_2.begin(SDA_2, SCL_2);


  mlx_ok  = mlx.begin();


  aht1_ok = aht10_1.begin(&Wire);
  aht2_ok = aht10_2.begin(&I2C_2);


  delay(1000);
  dht1.begin();
  dht2.begin();


  
  // para debug
  debug.mlxf(mlx_ok  ? "✅ MLX90614 listo (Wire)"        : "❌ MLX90614 no inicializó");
  debug.ahtf(aht1_ok ? "✅ AHT #1 listo (Wire)"          : "❌ AHT #1 falló");
  debug.ahtf(aht2_ok ? "✅ AHT #2 listo (Wire1)"         : "❌ AHT #2 falló");
  debug.dht("✅ DHT11 #1 y #2 inicializados");
  debug.infoSensor("=============================\n");
}
void configurar_LED(){
  pinMode(LED, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(LED, LOW);
  digitalWrite(RELAY_PIN, HIGH);
  delay(1000);
}
void setup() {
  Serial.begin(115200);
  tiempoInicial = millis();  // Guarda el tiempo inicial (al arrancar)
  wifi_setup();
  sensor_setup();
  configurar_LED();
  xTaskCreatePinnedToCore(sensorTask, "Sensor Task", 10000, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(webServerTask, "Web Server Task", 10000, NULL, 1, NULL, 1);
}

void loop() {
  // Las tareas corren en nucleos separados
  tiempoActual = millis() - tiempoInicial;
  tiempo=tiempoActual;
  tiempoFormato = formatoTiempo(tiempo);
}





