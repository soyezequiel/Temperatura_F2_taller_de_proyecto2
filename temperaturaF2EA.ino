#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <DHT.h>
#include <Adafruit_AHTX0.h>
#include <WiFi.h>

#define ENABLE_CHART 1   // ⇦ poné 0 para quitar la gráfica

#include "web_page_core.h"
#if ENABLE_CHART
  #include "web_chart.h"
#endif
#include "web_send.h"

// ===== CONFIGURACIÓN GLOBAL DE TIEMPOS =====

// Frecuencia de escaneo de sensores (en milisegundos)
#define SENSOR_INTERVAL_MS   1000   // cada 1 s

// Frecuencia de actualización de la gráfica y del fetch /sensordata (en ms)
#define WEB_UPDATE_MS        1000   // cada 1 s

// Timeout de cliente web (inactividad)
#define WEB_CLIENT_TIMEOUT_MS 2000  // 2 s sin tráfico = cortar conexión


//para chequear si el reinicio es por el wdt
// #include "esp_system.h"

// Configuracion Wi-Fi
const char* ssid = "electricidad";
const char* password = "medianoche extinto carreras asado hoja integral";
WiFiServer server(80);
String header;

// Pines y configuraciones de sensores
#define SDA_1 21
#define SCL_1 22
#define SDA_2 18
#define SCL_2 19
#define DHTPIN1 13
#define DHTPIN2 14
#define DHTTYPE DHT11
#define RELAY_PIN 15
#define LED 4


// Limites de temperatura
float tempLimit = 0.0;
float criticalTempLimit = 0.0;
bool tempLimitConfigured = false;
bool criticalTempLimitConfigured = false;

// Instancias de sensores
DHT dht1(DHTPIN1, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);

Adafruit_AHTX0 aht10_1;
Adafruit_AHTX0 aht10_2;
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
TwoWire I2C_2 = TwoWire(1);

// Variables de almacenamiento de datos
float ahtTemp1=0, ahtHum1=0, ahtTemp2=0, ahtHum2=0;
float dhtTemp1, dhtHum1, dhtTemp2, dhtHum2;
float dhtTemp1Aux, dhtHum1Aux, dhtTemp2Aux, dhtHum2Aux;

float mlxTempObj, mlxTempAmb;
bool relayState = false;
bool relayLocked = false;
int tiempo = 0;
String tiempoFormato = "00:00:00";

// Flags de inicialización
bool mlx_ok  = false;
bool aht1_ok = false;
bool aht2_ok = false;


//para chequear si el reinicio es por el wdt
// void printResetReason() {
//   esp_reset_reason_t r = esp_reset_reason();
//   Serial.print("Reset reason: ");
//   Serial.println(r); // ESP_RST_TASK_WDT=3, ESP_RST_WDT=4, etc.
// }

// Funcion para convertir float a String con formato controlado
String formatFloat(float value, int precision = 2) {
  char buffer[10] = "";
  dtostrf(value, 4, precision, buffer);
  return String(buffer);
}
//Funcion para convertir segundos a formato hh:mm:ss
String convertirASegundos(int segundosTotales) {
    int horas = segundosTotales / 3600;
    int minutos = (segundosTotales % 3600) / 60;
    int segundos = segundosTotales % 60;

    char tiempoFormato[9];
    sprintf(tiempoFormato, "%02d:%02d:%02d", horas, minutos, segundos);

    return String(tiempoFormato); // Convertir a string y devolver
}
bool leerDHT(DHT &dht, float &dhtTemp, float &dhtHum, const char *ubicacion="interno") {
    bool error;
    float dhtTempAux = dht.readTemperature();
    float dhtHumAux = dht.readHumidity();
    if (isnan(dhtTempAux) || isnan(dhtHumAux)) { Serial.printf("❌ DHT #%s lectura inválida",ubicacion); error = true; }
    else {
            dhtTemp = dhtTempAux;
            dhtHum = dhtHumAux;
            Serial.printf("DHT #(%s) → T: %.2f °C | H: %.2f %%\n",ubicacion, dhtTemp, dhtHum);
    }
    return error; 
}
bool leerAHT(Adafruit_AHTX0 &aht10, bool aht_ok, float  &temp, float  &humidity, const char *ubicacion="interno") {
    bool error;
    if (aht_ok) {
      sensors_event_t humidityEvent, tempEvent;
      aht10.getEvent(&humidityEvent, &tempEvent);
      if (isnan(tempEvent.temperature) || isnan(humidityEvent.relative_humidity)) {
        Serial.printf("❌ AHT #(%s) lectura inválida ",ubicacion); error = true;
        temp = -100;
        humidity = -100;
      } else {
        Serial.printf("AHT #(%s) → T: %.2f °C | H: %.2f %% \n",ubicacion, tempEvent.temperature, humidityEvent.relative_humidity);
            temp = tempEvent.temperature;
            humidity = humidityEvent.relative_humidity;
      }
    } else { Serial.printf("⚠️ AHT #(%s) no inicializado",ubicacion); error = true; 
        temp = 0;
        humidity = 0;
    }
    return error; 
}
bool leerMLX(Adafruit_MLX90614 &mlx, bool mlx_ok, float  &mlxTempObj, float  &mlxTempAmb, const char *ubicacion="interno") {
    bool error;
    // Lee omfrarojo
    mlxTempObj = 0;
    mlxTempAmb = 0;
    // ----- MLX90614 -----
    if (mlx_ok) {
      mlxTempObj = mlx.readObjectTempC();
      mlxTempAmb = mlx.readAmbientTempC();
      if (isnan(mlxTempAmb) || isnan(mlxTempObj)) { Serial.println("❌ MLX90614 lectura inválida"); error = true; }
      else Serial.printf("MLX90614 → Tamb: %.2f °C | Tobj: %.2f °C\n", mlxTempAmb, mlxTempObj);
    } else {
      Serial.println("⚠️ MLX90614 no inicializado");
      error = true;
    }
    return error; 
}
void releUpdate(){
    if (!relayLocked && tempLimitConfigured && criticalTempLimitConfigured && relayState) {
      if (mlxTempObj >= tempLimit) {
        //relayState = false;
        digitalWrite(RELAY_PIN, LOW);
        digitalWrite(LED, LOW);
        //Serial.println("Rele Apagado");
      } else if (mlxTempObj <= tempLimit - 5.00) {
        //relayState = true;
        digitalWrite(RELAY_PIN, HIGH);
        digitalWrite(LED, HIGH);
        //Serial.println("Rele Prendido");
        //Serial.println(tempLimit);
      }
    }
}
void releUpdateDesbloqueo(){
  if (ahtTemp1 >= criticalTempLimit && criticalTempLimitConfigured) {
    relayLocked = true;
    relayState = false;
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(LED, LOW);
    //Serial.println("Limite critico alcanzado. Rele apagado definitivamente.");
  }
}
// Funcion de tarea para el nucleo 1: leer sensores y controlar el rele
void leerSensores() {
  bool error = false;
  leerDHT(dht1, dhtTemp1, dhtHum1);
  leerDHT(dht2, dhtTemp2, dhtHum2, "externo");
  leerAHT(aht10_1, aht1_ok,ahtTemp1, ahtHum1);
  leerAHT(aht10_2, aht2_ok,ahtTemp2, ahtHum2, "externo");
  leerMLX(mlx,mlx_ok,mlxTempObj,mlxTempAmb);
  Serial.println("-----------------------------");
}
void sensorTask(void *pvParameters) {
  for(;;) {
    leerSensores();
    releUpdate();
    releUpdateDesbloqueo();
    tiempoFormato = convertirASegundos(tiempo);
    tiempo += 1; 
    vTaskDelay(pdMS_TO_TICKS(SENSOR_INTERVAL_MS));
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
                  digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
                  digitalWrite(LED, relayState ? HIGH : LOW);
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

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConexion establecida.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();

  Wire.begin(SDA_1, SCL_1);
  I2C_2.begin(SDA_2, SCL_2);




  aht1_ok = aht10_1.begin(&Wire);
  aht2_ok = aht10_2.begin(&I2C_2);
  mlx_ok  = mlx.begin();
  dht1.begin();
  dht2.begin();

  // para debug
  Serial.println(mlx_ok  ? "✅ MLX90614 listo (Wire)"        : "❌ MLX90614 no inicializó");
  Serial.println(aht1_ok ? "✅ AHT #1 listo (Wire)"          : "❌ AHT #1 falló");
  Serial.println(aht2_ok ? "✅ AHT #2 listo (Wire1)"         : "❌ AHT #2 falló");
  Serial.println("✅ DHT11 #1 y #2 inicializados");
  Serial.println("=============================\n");


  pinMode(LED, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(LED, LOW);
  digitalWrite(RELAY_PIN, LOW);
  delay(1000);
  //xTaskCreatePinnedToCore(sensorTask, "Sensor Task", 10000, NULL, 1, NULL, 0);
  //xTaskCreatePinnedToCore(webServerTask, "Web Server Task", 10000, NULL, 1, NULL, 1);

  //intento de arreglar el dht
  xTaskCreatePinnedToCore(sensorTask, "Sensor Task", 10000, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(webServerTask, "Web Server Task", 10000, NULL, 1, NULL, 1);

  //para chequear si el reinicio es por el wdt
  // delay(2000);
  // printResetReason();
}

void loop() {
  // Las tareas corren en nucleos separados
}
