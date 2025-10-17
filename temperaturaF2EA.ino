#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <DHT.h>
#include <Adafruit_AHTX0.h>
#include <WiFi.h>

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
#define DHTPIN2 12
#define DHTTYPE DHT11
#define RELAY_PIN 15
#define LED 2

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

// Funcion de tarea para el nucleo 1: leer sensores y controlar el rele
void sensorTask(void *pvParameters) {
  for (;;) {
    sensors_event_t humidity1, temp1, humidity2, temp2;
    aht10_1.getEvent(&humidity1, &temp1);
    aht10_2.getEvent(&humidity2, &temp2);

    ahtTemp1 = temp1.temperature;
    ahtHum1 = humidity1.relative_humidity;
    ahtTemp2 = temp2.temperature;
    ahtHum2 = humidity2.relative_humidity;

    /*
    myDelay(5);
    dhtTemp1Aux = dht1.readTemperature();
    myDelay(5);
    dhtHum1Aux = dht1.readHumidity();
    myDelay(5);
    dhtTemp2Aux = dht2.readTemperature();
    myDelay(5);
    dhtHum2Aux = dht2.readHumidity();

    if (!isnan(dhtTemp1Aux) && !isnan(dhtHum1Aux)) {
      dhtTemp1 = dhtTemp1Aux;
      dhtHum1 = dhtHum1Aux;
    } else {
      Serial.println("Error en DHT1");
    }
    if (!isnan(dhtTemp2Aux) && !isnan(dhtHum2Aux)) {
      dhtTemp2 = dhtTemp2Aux;
      dhtHum2 = dhtHum2Aux;
    } else {
      Serial.println("Error en DHT2");
    }
    */

    mlxTempObj = mlx.readObjectTempC();
    mlxTempAmb = mlx.readAmbientTempC();

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

                // Crear la respuesta JSON
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

                // respuesta JSON
                client.print(jsonResponse);
                //Serial.println("Respuesta JSON: " + jsonResponse);
                break;
              }

              if (header.indexOf("GET /toggleRelay") >= 0) {
                if (!relayLocked && tempLimitConfigured && criticalTempLimitConfigured) {
                  relayState = !relayState; // Cambiar el estado del rele
                  digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
                  digitalWrite(LED, relayState ? HIGH : LOW);
                }
                client.println("HTTP/1.1 200 OK");
                client.println("Connection: close");
                client.println();
              }

              // Actualizacion de limites a traves de la URL
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

              // HTML y JavaScript
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

              // Botones para seleccionar el modo
              client.println("<button onclick='showExperiment()'>Monitor</button>");
              client.println("<button onclick='showCSVGraph()'>Graficar CSV</button>");

              // Configuracion de limites
              client.println("<div id='experimentInterface' style='display: block;'>");

              client.println("<h2>Configuracion de Limites</h2>");
              client.println("<p id='tempLimitDisplay'>Temperatura de calentamiento (MLX90614): " + (tempLimitConfigured ? formatFloat(tempLimit, 2) + "°C" : "No configurado") + "</p>");
              client.println("<form id='setLimitForm' onsubmit='submitLimitForm(event, \"/setlimit/\", \"tempLimitDisplay\", \"tempLimitMessage\")'>");
              client.println("<input type='number' id='setLimitInput' name='temp' min='0' max='90' step='0.01' placeholder='0.00 - 90.00 °C' required>");
              client.println("<input type='submit' value='Actualizar Limite'>");
              client.println("</form>");
              client.println("<p id='tempLimitMessage' style='color: red;'></p>"); // Mensaje de validacion o exito

              client.println("<p id='criticalTempLimitDisplay'>Temperatura a la que se quiere llegar (AHT10): " + (criticalTempLimitConfigured ? formatFloat(criticalTempLimit, 2) + "°C" : "No configurado") + "</p>");
              client.println("<form id='setCriticalLimitForm' onsubmit='submitLimitForm(event, \"/setcriticallimit/\", \"criticalTempLimitDisplay\", \"criticalTempLimitMessage\")'>");
              client.println("<input type='number' id='setCriticalLimitInput' name='temp' min='0' max='45' step='0.01' placeholder='0.00 - 45.00 °C' required>");
              client.println("<input type='submit' value='Actualizar Limite'>");
              client.println("</form>");
              client.println("<p id='criticalTempLimitMessage' style='color: red;'></p>"); // Mensaje de validacion o exito

              // JavaScript para manejar el envio asincrono y la validacion
              client.println("<script>");
              client.println("function submitLimitForm(event, endpoint, displayId, messageId) {");
              client.println("  event.preventDefault();"); // Prevenir el envio tradicional del formulario
              client.println("  var input = event.target.querySelector('input[name=\"temp\"]');");
              client.println("  var value = parseFloat(input.value);");
              client.println("  var min = parseFloat(input.min);");
              client.println("  var max = parseFloat(input.max);");
              client.println("  var messageElement = document.getElementById(messageId);");

              client.println("  // Validar el rango del valor");
              client.println("  if (value < min || value > max) {");
              client.println("    messageElement.style.color = 'red';");
              client.println("    messageElement.innerText = 'El valor debe estar entre ' + min + '°C y ' + max + '°C.';");
              client.println("    return;");
              client.println("  }");

              client.println("  var xhttp = new XMLHttpRequest();");
              client.println("  xhttp.onreadystatechange = function() {");
              client.println("    if (this.readyState == 4 && this.status == 200) {");
              client.println("      // Actualizar el elemento de la interfaz con el nuevo valor");
              client.println("      document.getElementById(displayId).innerText = 'Temperatura actualizada: ' + value.toFixed(2) + '°C';");
              client.println("    }");
              client.println("  };");
              client.println("  xhttp.open('GET', endpoint + '?temp=' + value.toFixed(2), true);");
              client.println("  xhttp.send();");
              client.println("}");
              client.println("</script>");


              // Control del rele
              client.println("<h2>Control del Rele</h2>");
              client.println("<p id='relayMessage'>Cargando estado...</p>");
              client.println("<button id='relayButton' onclick='toggleRelay()'>Encender Rele</button>");

              // Lecturas de sensores
              client.println("<h2>Lecturas de Sensores</h2>");
              client.println("<p>MLX90614 - Temp Objeto: <span id='mlxTempObj'>--°C</span>, Temp Ambiente: <span id='mlxTempAmb'>--°C</span></p>");
              client.println("<p>AHT10 Interno - Temp: <span id='ahtTemp1'>--°C</span>, Humedad: <span id='ahtHum1'>--%</span></p>");
              client.println("<p>DHT11 Interno - Temp: <span id='dhtTemp2'>--°C</span>, Humedad: <span id='dhtHum2'>--%</span></p>");
              client.println("<p>AHT10 Externo - Temp: <span id='ahtTemp2'>--°C</span>, Humedad: <span id='ahtHum2'>--%</span></p>");
              client.println("<p>DHT11 Externo - Temp: <span id='dhtTemp1'>--°C</span>, Humedad: <span id='dhtHum1'>--%</span></p>");

              // Botones para exportar CSV
              client.println("<button onclick='exportCSV()'>Exportar a CSV</button>");

              // Div para la gráfica de Plotly
              client.println("<div id='plotlyChartContainer' style='display: flex; justify-content: center; align-items: center; width: 100%;'>");
              client.println("  <div id='plotlyChart' style='width: 100%; max-width: 900px; height: 600px;'></div>");
              client.println("</div>");
              client.println("</div>"); // Cierre de experimentInterface


              // Variables de almacenamiento y generación de gráficos
              client.println("<script>");
              client.println("let csvData = [['Tiempo', 'AHT10 Interno', 'AHT10 Externo', 'MLX90614 Objeto', 'MLX90614 Ambiente']];");

              // Función para actualizar la gráfica de Plotly y almacenar datos en `csvData`
              client.println("let lastTime = '';");  // Variable para almacenar el último tiempo registrado
              client.println("function updatePlotlyChart() {");
              client.println("  var xhttp = new XMLHttpRequest();");
              client.println("  xhttp.onreadystatechange = function() {");
              client.println("    if (this.readyState == 4 && this.status == 200) {");
              client.println("      var data = JSON.parse(this.responseText);");

              client.println("      // Verificar si el nuevo tiempo es diferente del último registrado");
              client.println("      if (data.tiempo !== lastTime) {");
              client.println("        // Agregar datos a csvData para exportación y actualizar lastTime");
              client.println("        csvData.push([data.tiempo, data.ahtTemp1, data.ahtTemp2, data.mlxTempObj, data.mlxTempAmb]);");
              client.println("        lastTime = data.tiempo;");  // Actualizar el último tiempo registrado

              client.println("        var update = {");
              client.println("          x: [[data.tiempo], [data.tiempo], [data.tiempo], [data.tiempo]],");
              client.println("          y: [[data.ahtTemp1], [data.ahtTemp2], [data.mlxTempObj], [data.mlxTempAmb]]");
              client.println("        };");
              client.println("        Plotly.extendTraces('plotlyChart', update, [0, 1, 2, 3]);");
              client.println("      }");
              client.println("    }");
              client.println("  };");
              client.println("  xhttp.open('GET', '/sensordata', true);");
              client.println("  xhttp.send();");
              client.println("}");
              client.println("setInterval(updatePlotlyChart, 1000);");

              // Función para importar CSV y actualizar la gráfica
              client.println("function importCSV(event) {");
              client.println("  var file = event.target.files[0];");
              client.println("  if (file) {");
              client.println("    var reader = new FileReader();");
              client.println("    reader.onload = function(e) {");
              client.println("      var rows = e.target.result.split('\\n').slice(1);");
              client.println("      var x = [], yInterno = [], yExterno = [], yObj = [], yAmb = [];");
              client.println("      rows.forEach(row => {");
              client.println("        var cols = row.split(',');");
              client.println("        x.push(cols[0]); yInterno.push(parseFloat(cols[1])); yExterno.push(parseFloat(cols[2]));");
              client.println("        yObj.push(parseFloat(cols[3])); yAmb.push(parseFloat(cols[4]));");
              client.println("      });");
              client.println("      Plotly.newPlot('plotlyChart', [");
              client.println("        { x: x, y: yInterno, mode: 'lines', name: 'AHT10 Interno', line: {color: 'red'} },");
              client.println("        { x: x, y: yExterno, mode: 'lines', name: 'AHT10 Externo', line: {color: 'blue'} },");
              client.println("        { x: x, y: yObj, mode: 'lines', name: 'MLX90614 Objeto', line: {color: 'green'} },");
              client.println("        { x: x, y: yAmb, mode: 'lines', name: 'MLX90614 Ambiente', line: {color: 'orange'} }");
              client.println("      ], { title: 'Grafica', xaxis: { title: 'Tiempo' }, yaxis: { title: 'Temperatura (°C)' } });");
              client.println("    };");
              client.println("    reader.readAsText(file);");
              client.println("  }");
              client.println("}");

              // Función para exportar datos a CSV
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

              // Configuración inicial de la gráfica de Plotly con menú de selección
              client.println("<script>");
              client.println("Plotly.newPlot('plotlyChart', [{");
              client.println("  x: [],");
              client.println("  y: [],");
              client.println("  mode: 'lines',");
              client.println("  name: 'AHT10 Interno'");
              client.println("}, {");
              client.println("  x: [],");
              client.println("  y: [],");
              client.println("  mode: 'lines',");
              client.println("  name: 'AHT10 Externo'");
              client.println("}, {");
              client.println("  x: [],");
              client.println("  y: [],");
              client.println("  mode: 'lines',");
              client.println("  name: 'MLX90614 Objeto'");
              client.println("}, {");
              client.println("  x: [],");
              client.println("  y: [],");
              client.println("  mode: 'lines',");
              client.println("  name: 'MLX90614 Ambiente'");
              client.println("}], {");
              client.println("  title: 'Grafica de Temperaturas de Sensores',");
              client.println("  xaxis: { title: 'Tiempo' },");
              client.println("  yaxis: { title: 'Temperatura (°C)' }");
              client.println("}, {locale: 'es', displaylogo: false , responsive: true, displayModeBar: true});");
              client.println("</script>");

              // Contenedor para solo graficar CSV
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
              client.println("      var x = [], yInterno = [], yExterno = [], yObj = [], yAmb = [];");
              client.println("      rows.forEach(row => {");
              client.println("        var cols = row.split(',');");
              client.println("        x.push(cols[0]); yInterno.push(parseFloat(cols[1])); yExterno.push(parseFloat(cols[2]));");
              client.println("        yObj.push(parseFloat(cols[3])); yAmb.push(parseFloat(cols[4]));");
              client.println("      });");
              client.println("      Plotly.newPlot('csvPlotlyChart', [");
              client.println("        { x: x, y: yInterno, mode: 'lines', name: 'AHT10 Interno' },");
              client.println("        { x: x, y: yExterno, mode: 'lines', name: 'AHT10 Externo' },");
              client.println("        { x: x, y: yObj, mode: 'lines', name: 'MLX90614 Objeto' },");
              client.println("        { x: x, y: yAmb, mode: 'lines', name: 'MLX90614 Ambiente' }");
              client.println("      ], { title: 'Grafica desde CSV', xaxis: { title: 'Tiempo' }, yaxis: { title: 'Temperatura (°C)' }}, {locale: 'es', displaylogo: false , responsive: true , displayModeBar: true});");
              client.println("    };");
              client.println("    reader.readAsText(file);");
              client.println("  }");
              client.println("}");
              client.println("</script>");
              client.println("</div>"); // Cierre de csvInterface

              // Funciones JavaScript para alternar vistas
              client.println("<script>");
              client.println("function showExperiment() {");
              client.println("  document.getElementById('experimentInterface').style.display = 'block';");
              client.println("  document.getElementById('csvInterface').style.display = 'none';");
              client.println("}");
              client.println("function showCSVGraph() {");
              client.println("  document.getElementById('experimentInterface').style.display = 'none';");
              client.println("  document.getElementById('csvInterface').style.display = 'block';");
              client.println("}");
              client.println("</script>");
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
  aht10_1.begin(&Wire);
  aht10_2.begin(&I2C_2);
  mlx.begin();
  dht1.begin();
  dht2.begin();
  pinMode(LED, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(LED, LOW);
  digitalWrite(RELAY_PIN, LOW);
  delay(1000);
  xTaskCreatePinnedToCore(sensorTask, "Sensor Task", 10000, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(webServerTask, "Web Server Task", 10000, NULL, 1, NULL, 1);
}

void loop() {
  // Las tareas corren en nucleos separados
}
