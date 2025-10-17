#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <DHT.h>
#include <Adafruit_AHTX0.h>
#include <WiFi.h>

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

const int INTERVALO = 1000;     // ms entre lecturas
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


void sensorTask(void *pvParameters) {
  for(;;) {
    leerSensores();
    vTaskDelay(1000 / portTICK_PERIOD_MS); // duerme 1 segundo sin bloquear CPU
  }
}

bool leerDHT(DHT &dht, float &dhtTemp, float &dhtHum) {
    bool error;
    vTaskDelay(200 / portTICK_PERIOD_MS); // duerme x segundo sin bloquear CPU
    float dhtTempAux = dht.readTemperature();
    vTaskDelay(200 / portTICK_PERIOD_MS); // duerme x segundo sin bloquear CPU
    float dhtHumAux = dht.readHumidity();
    if (isnan(dhtTempAux) || isnan(dhtHumAux)) { Serial.println("❌ DHT #1 lectura inválida"); error = true; }
    else {
            dhtTemp = dhtTempAux;
            dhtHum = dhtHumAux;
            Serial.printf("DHT #(interno) → T: %.2f °C | H: %.2f %%\n", dhtTemp, dhtHum);
    }
    return error; 
}
bool leerAHT(Adafruit_AHTX0 &aht10, bool aht_ok, float  &temp, float  &humidity, const char *ubicacion) {
    bool error;
    if (aht_ok) {
      sensors_event_t humidityEvent, tempEvent;
      aht10.getEvent(&humidityEvent, &tempEvent);
      if (isnan(tempEvent.temperature) || isnan(humidityEvent.relative_humidity)) {
        Serial.println("❌ AHT #(interno) lectura inválida "); error = true;
        temp = -100;
        humidity = -100;
      } else {
        Serial.printf("AHT #(%s) → T: %.2f °C | H: %.2f %% \n",ubicacion, tempEvent.temperature, humidityEvent.relative_humidity);
            temp = tempEvent.temperature;
            humidity = humidityEvent.relative_humidity;
      }
    } else { Serial.println("⚠️ AHT #(interno) no inicializado"); error = true; 
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

// Funcion de tarea para el nucleo 1: leer sensores y controlar el rele
void leerSensores() {
  bool error = false;
  for (;;) {
    leerDHT(dht1, dhtTemp1, dhtHum1);
    leerDHT(dht2, dhtTemp2, dhtHum2);
    leerAHT(aht10_1, aht1_ok,ahtTemp1, ahtHum1, "interno");
    leerAHT(aht10_2, aht2_ok,ahtTemp2, ahtHum2, "externo");
    leerMLX(mlx,mlx_ok,mlxTempObj,mlxTempAmb);

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

    if (ahtTemp1 >= criticalTempLimit && criticalTempLimitConfigured) {
      relayLocked = true;
      relayState = false;
      digitalWrite(RELAY_PIN, LOW);
      digitalWrite(LED, LOW);
      //Serial.println("Limite critico alcanzado. Rele apagado definitivamente.");
    }
    tiempoFormato = convertirASegundos(tiempo);
    tiempo += 1; 
    vTaskDelay(pdMS_TO_TICKS(1000)); // Retraso de 1 segundo en la tarea

    Serial.println("-----------------------------");
    //delay(INTERVALO);
  }
}

// Funcion de tarea para el nucleo 2: gestionar la interfaz web
void webServerTask(void *pvParameters) {
  for (;;) {
    WiFiClient client = server.available();
    if (client) {
      String currentLine = "";
      while (client.connected()) {
        if (client.available()) {
          char c = client.read();
          header += c;
          if (c == '\n') {
            if (currentLine.length() == 0) {
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

              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println("Connection: close");
              client.println();

              client.println("<!DOCTYPE html><html lang='es'><head>");
              client.println("<meta charset='UTF-8'>");
              client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
              client.println("<script src='https://cdn.plot.ly/plotly-latest.min.js'></script>");
              client.println("<script src='https://cdn.plot.ly/plotly-locale-es-latest.js'></script>");
              client.println("<style>body {font-family: Arial; text-align: center;}</style></head><body>");
              client.println("<h1>Heat Transfer - Monitor y Control de Rele</h1>");
              client.println("<script>");
              client.println("function updateSensorData() {");
              client.println("  var xhttp = new XMLHttpRequest();");
              client.println("  xhttp.onreadystatechange = function() {");
              client.println("    if (this.readyState == 4 && this.status == 200) {");
              client.println("      var data = JSON.parse(this.responseText);");
              client.println("      document.getElementById('ahtTemp1').innerHTML = data.ahtTemp1 + '°C';");
              client.println("      document.getElementById('ahtHum1').innerHTML = data.ahtHum1 + '%';");
              client.println("      document.getElementById('ahtTemp2').innerHTML = data.ahtTemp2 + '°C';");
              client.println("      document.getElementById('ahtHum2').innerHTML = data.ahtHum2 + '%';");
              client.println("      document.getElementById('dhtTemp1').innerHTML = data.dhtTemp1 + '°C';");
              client.println("      document.getElementById('dhtHum1').innerHTML = data.dhtHum1 + '%';");
              client.println("      document.getElementById('dhtTemp2').innerHTML = data.dhtTemp2 + '°C';");
              client.println("      document.getElementById('dhtHum2').innerHTML = data.dhtHum2 + '%';");
              client.println("      document.getElementById('mlxTempObj').innerHTML = data.mlxTempObj + '°C';");
              client.println("      document.getElementById('mlxTempAmb').innerHTML = data.mlxTempAmb + '°C';");
              client.println("      document.getElementById('relayButton').innerHTML = data.relayState ? 'Apagar Rele' : 'Encender Rele';");
              client.println("      document.getElementById('relayMessage').innerHTML = data.relayMessage;");
              client.println("    }");
              client.println("  };");
              client.println("  xhttp.open('GET', '/sensordata', true);");
              client.println("  xhttp.send();");
              client.println("}");
              client.println("function toggleRelay() {");
              client.println("  var xhttp = new XMLHttpRequest();");
              client.println("  xhttp.open('GET', '/toggleRelay', true);");
              client.println("  xhttp.send();");
              client.println("}");
              client.println("setInterval(updateSensorData, 1000);");
              client.println("</script>");

              client.println("<button onclick='showExperiment()'>Monitor</button>");
              client.println("<button onclick='showCSVGraph()'>Graficar CSV</button>");

              client.println("<div id='experimentInterface' style='display: block;'>");

              client.println("<h2>Configuracion de Limites</h2>");
              client.println("<p id='tempLimitDisplay'>Temperatura de calentamiento (MLX90614): " + (tempLimitConfigured ? formatFloat(tempLimit, 2) + "°C" : "No configurado") + "</p>");
              client.println("<form id='setLimitForm' onsubmit='submitLimitForm(event, \"/setlimit/\", \"tempLimitDisplay\", \"tempLimitMessage\")'>");
              client.println("<input type='number' id='setLimitInput' name='temp' min='0' max='90' step='0.01' placeholder='0.00 - 90.00 °C' required>");
              client.println("<input type='submit' value='Actualizar Limite'>");
              client.println("</form>");
              client.println("<p id='tempLimitMessage' style='color: red;'></p>");

              client.println("<p id='criticalTempLimitDisplay'>Temperatura a la que se quiere llegar (AHT10): " + (criticalTempLimitConfigured ? formatFloat(criticalTempLimit, 2) + "°C" : "No configurado") + "</p>");
              client.println("<form id='setCriticalLimitForm' onsubmit='submitLimitForm(event, \"/setcriticallimit/\", \"criticalTempLimitDisplay\", \"criticalTempLimitMessage\")'>");
              client.println("<input type='number' id='setCriticalLimitInput' name='temp' min='0' max='45' step='0.01' placeholder='0.00 - 45.00 °C' required>");
              client.println("<input type='submit' value='Actualizar Limite'>");
              client.println("</form>");
              client.println("<p id='criticalTempLimitMessage' style='color: red;'></p>");

              client.println("<script>");
              client.println("function submitLimitForm(event, endpoint, displayId, messageId) {");
              client.println("  event.preventDefault();");
              client.println("  var input = event.target.querySelector('input[name=\"temp\"]');");
              client.println("  var value = parseFloat(input.value);");
              client.println("  var min = parseFloat(input.min);");
              client.println("  var max = parseFloat(input.max);");
              client.println("  var messageElement = document.getElementById(messageId);");
              client.println("  if (value < min || value > max) {");
              client.println("    messageElement.style.color = 'red';");
              client.println("    messageElement.innerText = 'El valor debe estar entre ' + min + '°C y ' + max + '°C.';");
              client.println("    return;");
              client.println("  }");
              client.println("  var xhttp = new XMLHttpRequest();");
              client.println("  xhttp.onreadystatechange = function() {");
              client.println("    if (this.readyState == 4 && this.status == 200) {");
              client.println("      document.getElementById(displayId).innerText = 'Temperatura actualizada: ' + value.toFixed(2) + '°C';");
              client.println("    }");
              client.println("  };");
              client.println("  xhttp.open('GET', endpoint + '?temp=' + value.toFixed(2), true);");
              client.println("  xhttp.send();");
              client.println("}");
              client.println("</script>");

              client.println("<h2>Control del Rele</h2>");
              client.println("<p id='relayMessage'>Cargando estado...</p>");
              client.println("<button id='relayButton' onclick='toggleRelay()'>Encender Rele</button>");

              client.println("<h2>Lecturas de Sensores</h2>");
              client.println("<p>MLX90614 - Temp Objeto: <span id='mlxTempObj'>--°C</span>, Temp Ambiente: <span id='mlxTempAmb'>--°C</span></p>");
              client.println("<p>AHT10 Interno - Temp: <span id='ahtTemp1'>--°C</span>, Humedad: <span id='ahtHum1'>--%</span></p>");
              client.println("<p>DHT11 Interno - Temp: <span id='dhtTemp2'>--°C</span>, Humedad: <span id='dhtHum2'>--%</span></p>");
              client.println("<p>AHT10 Externo - Temp: <span id='ahtTemp2'>--°C</span>, Humedad: <span id='ahtHum2'>--%</span></p>");
              client.println("<p>DHT11 Externo - Temp: <span id='dhtTemp1'>--°C</span>, Humedad: <span id='dhtHum1'>--%</span></p>");

              client.println("<button onclick='exportCSV()'>Exportar a CSV</button>");

              client.println("<div id='plotlyChartContainer' style='display: flex; justify-content: center; align-items: center; width: 100%;'>");
              client.println("  <div id='plotlyChart' style='width: 100%; max-width: 900px; height: 600px;'></div>");
              client.println("</div>");
              client.println("</div>");

              client.println("<script>");
              client.println("let csvData = [['Tiempo', 'AHT10 Interno', 'AHT10 Externo', 'DHT11 Interno', 'DHT11 Externo', 'MLX90614 Objeto', 'MLX90614 Ambiente']];");

              client.println("let lastTime = '';");
              client.println("function updatePlotlyChart() {");
              client.println("  var xhttp = new XMLHttpRequest();");
              client.println("  xhttp.onreadystatechange = function() {");
              client.println("    if (this.readyState == 4 && this.status == 200) {");
              client.println("      var data = JSON.parse(this.responseText);");
              client.println("      if (data.tiempo !== lastTime) {");
              client.println("        csvData.push([data.tiempo, data.ahtTemp1, data.ahtTemp2, data.dhtTemp2, data.dhtTemp1, data.mlxTempObj, data.mlxTempAmb]);");
              client.println("        lastTime = data.tiempo;");
              client.println("        var update = {");
              client.println("          x: [[data.tiempo], [data.tiempo], [data.tiempo], [data.tiempo], [data.tiempo], [data.tiempo]],");
              client.println("          y: [[data.ahtTemp1], [data.ahtTemp2], [data.dhtTemp2], [data.dhtTemp1], [data.mlxTempObj], [data.mlxTempAmb]]");
              client.println("        };");
              client.println("        Plotly.extendTraces('plotlyChart', update, [0, 1, 2, 3, 4, 5]);");
              client.println("      }");
              client.println("    }");
              client.println("  };");
              client.println("  xhttp.open('GET', '/sensordata', true);");
              client.println("  xhttp.send();");
              client.println("}");
              client.println("setInterval(updatePlotlyChart, 1000);");

              client.println("function exportCSV() {");
              client.println("  let csvContent = 'data:text/csv;charset=utf-8,' + csvData.map(e => e.join(',')).join('\\n');");
              client.println("  var link = document.createElement('a');");
              client.println("  link.setAttribute('href', encodeURI(csvContent));");
              client.println("  link.setAttribute('download', 'Heat_Transfer.csv');");
              client.println("  document.body.appendChild(link);");
              client.println("  link.click();");
              client.println("  document.body.removeChild(link);");
              client.println("}");
              client.println("</script>");

              client.println("<script>");
              client.println("Plotly.newPlot('plotlyChart', [{");
              client.println("  x: [], y: [], mode: 'lines', name: 'AHT10 Interno', line: {color: 'red'}");
              client.println("}, {");
              client.println("  x: [], y: [], mode: 'lines', name: 'AHT10 Externo', line: {color: 'blue'}");
              client.println("}, {");
              client.println("  x: [], y: [], mode: 'lines', name: 'DHT11 Interno', line: {color: 'purple'}");
              client.println("}, {");
              client.println("  x: [], y: [], mode: 'lines', name: 'DHT11 Externo', line: {color: 'orange'}");
              client.println("}, {");
              client.println("  x: [], y: [], mode: 'lines', name: 'MLX90614 Objeto', line: {color: 'green'}");
              client.println("}, {");
              client.println("  x: [], y: [], mode: 'lines', name: 'MLX90614 Ambiente', line: {color: 'brown'}");
              client.println("}], {");
              client.println("  title: 'Grafica de Temperaturas de Sensores',");
              client.println("  xaxis: { title: 'Tiempo' },");
              client.println("  yaxis: { title: 'Temperatura (°C)' }");
              client.println("}, {locale: 'es', displaylogo: false, responsive: true, displayModeBar: true});");
              client.println("</script>");

              client.println("<div id='csvInterface' style='display: none;'>");
              client.println("<h2>Importar CSV y Graficar</h2>");
              client.println("<input type='file' id='importCSVInput' accept='.csv' onchange='importCSV(event)'>");
              client.println("<div id='csvPlotlyChart' style='width: 90%; height: 500px; margin: auto;'></div>");
              
              client.println("<script>");
              client.println("function importCSV(event) {");
              client.println("  var file = event.target.files[0];");
              client.println("  if (file) {");
              client.println("    var reader = new FileReader();");
              client.println("    reader.onload = function(e) {");
              client.println("      var rows = e.target.result.split('\\n').slice(1);");
              client.println("      var x = [], yAHT1 = [], yAHT2 = [], yDHT2 = [], yDHT1 = [], yObj = [], yAmb = [];");
              client.println("      rows.forEach(row => {");
              client.println("        var cols = row.split(',');");
              client.println("        if (cols.length >= 7) {");
              client.println("          x.push(cols[0]);");
              client.println("          yAHT1.push(parseFloat(cols[1]));");
              client.println("          yAHT2.push(parseFloat(cols[2]));");
              client.println("          yDHT2.push(parseFloat(cols[3]));");
              client.println("          yDHT1.push(parseFloat(cols[4]));");
              client.println("          yObj.push(parseFloat(cols[5]));");
              client.println("          yAmb.push(parseFloat(cols[6]));");
              client.println("        }");
              client.println("      });");
              client.println("      Plotly.newPlot('csvPlotlyChart', [");
              client.println("        { x: x, y: yAHT1, mode: 'lines', name: 'AHT10 Interno', line: {color: 'red'} },");
              client.println("        { x: x, y: yAHT2, mode: 'lines', name: 'AHT10 Externo', line: {color: 'blue'} },");
              client.println("        { x: x, y: yDHT2, mode: 'lines', name: 'DHT11 Interno', line: {color: 'purple'} },");
              client.println("        { x: x, y: yDHT1, mode: 'lines', name: 'DHT11 Externo', line: {color: 'orange'} },");
              client.println("        { x: x, y: yObj, mode: 'lines', name: 'MLX90614 Objeto', line: {color: 'green'} },");
              client.println("        { x: x, y: yAmb, mode: 'lines', name: 'MLX90614 Ambiente', line: {color: 'brown'} }");
              client.println("      ], { title: 'Grafica desde CSV', xaxis: { title: 'Tiempo' }, yaxis: { title: 'Temperatura (°C)' } }, {locale: 'es', displaylogo: false, responsive: true, displayModeBar: true});");
              client.println("    };");
              client.println("    reader.readAsText(file);");
              client.println("  }");
              client.println("}");
              
              client.println("function showExperiment() {");
              client.println("  document.getElementById('experimentInterface').style.display = 'block';");
              client.println("  document.getElementById('csvInterface').style.display = 'none';");
              client.println("}");
              
              client.println("function showCSVGraph() {");
              client.println("  document.getElementById('experimentInterface').style.display = 'none';");
              client.println("  document.getElementById('csvInterface').style.display = 'block';");
              client.println("}");
              client.println("</script>");
              
              client.println("</div>");
              client.println("</body></html>");

              break;
            } else {
              currentLine = "";
            }
          } else if (c != '\r') {
            currentLine += c;
          }
        }
      }
      header = "";
      client.stop();
    }
    vTaskDelay(pdMS_TO_TICKS(10));
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
  xTaskCreatePinnedToCore(sensorTask, "Sensor Task", 10000, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(webServerTask, "Web Server Task", 10000, NULL, 1, NULL, 1);

  //para chequear si el reinicio es por el wdt
  // delay(2000);
  // printResetReason();
}

void loop() {
  // Las tareas corren en nucleos separados

}
