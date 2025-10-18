#pragma once
#include <Arduino.h>

const char index_html[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang='es'><head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<script src='https://cdn.plot.ly/plotly-latest.min.js'></script>
<script src='https://cdn.plot.ly/plotly-locale-es-latest.js'></script>
<style>body {font-family: Arial; text-align: center;}</style></head><body>
<h1>Heat Transfer - Monitor y Control de Rele</h1>
<script>
function updateSensorData() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      var data = JSON.parse(this.responseText);
      document.getElementById('ahtTemp1').innerHTML = data.ahtTemp1 + '°C';
      document.getElementById('ahtHum1').innerHTML = data.ahtHum1 + '%';
      document.getElementById('ahtTemp2').innerHTML = data.ahtTemp2 + '°C';
      document.getElementById('ahtHum2').innerHTML = data.ahtHum2 + '%';
      document.getElementById('dhtTemp1').innerHTML = data.dhtTemp1 + '°C';
      document.getElementById('dhtHum1').innerHTML = data.dhtHum1 + '%';
      document.getElementById('dhtTemp2').innerHTML = data.dhtTemp2 + '°C';
      document.getElementById('dhtHum2').innerHTML = data.dhtHum2 + '%';
      document.getElementById('mlxTempObj').innerHTML = data.mlxTempObj + '°C';
      document.getElementById('mlxTempAmb').innerHTML = data.mlxTempAmb + '°C';
      document.getElementById('relayButton').innerHTML = data.relayState ? 'Apagar Rele' : 'Encender Rele';
      document.getElementById('relayMessage').innerHTML = data.relayMessage;
    }
  };
  xhttp.open('GET', '/sensordata', true);
  xhttp.send();
}
function toggleRelay() {
  var xhttp = new XMLHttpRequest();
  xhttp.open('GET', '/toggleRelay', true);
  xhttp.send();
}
setInterval(updateSensorData, 1000);
</script>

<button onclick='showExperiment()'>Monitor</button>
<button onclick='showCSVGraph()'>Graficar CSV</button>

<div id='experimentInterface' style='display: block;'>

<h2>Configuracion de Limites</h2>

<!-- <p id='tempLimitDisplay'>Temperatura de calentamiento (MLX90614): " + (tempLimitConfigured ? formatFloat(tempLimit, 2) + "°C" : "No configurado") + "</p> -->
<p id='tempLimitDisplay'>Temperatura de calentamiento (MLX90614): <span id="tempLimitValue">No configurado</span></p>

<form id='setLimitForm' onsubmit='submitLimitForm(event, \"/setlimit/\", \"tempLimitDisplay\", \"tempLimitMessage\")'>
<input type='number' id='setLimitInput' name='temp' min='0' max='90' step='0.01' placeholder='0.00 - 90.00 °C' required>
<input type='submit' value='Actualizar Limite'>
</form>
<p id='tempLimitMessage' style='color: red;'></p>

<p id='criticalTempLimitDisplay'>Temperatura a la que se quiere llegar (AHT10): " + (criticalTempLimitConfigured ? formatFloat(criticalTempLimit, 2) + "°C" : "No configurado") + "</p>
<form id='setCriticalLimitForm' onsubmit='submitLimitForm(event, \"/setcriticallimit/\", \"criticalTempLimitDisplay\", \"criticalTempLimitMessage\")'>
<input type='number' id='setCriticalLimitInput' name='temp' min='0' max='45' step='0.01' placeholder='0.00 - 45.00 °C' required>
<input type='submit' value='Actualizar Limite'>
</form>
<p id='criticalTempLimitMessage' style='color: red;'></p>

<script>
function submitLimitForm(event, endpoint, displayId, messageId) {
  event.preventDefault();
  var input = event.target.querySelector('input[name=\"temp\"]');
  var value = parseFloat(input.value);
  var min = parseFloat(input.min);
  var max = parseFloat(input.max);
  var messageElement = document.getElementById(messageId);
  if (value < min || value > max) {
    messageElement.style.color = 'red';
    messageElement.innerText = 'El valor debe estar entre ' + min + '°C y ' + max + '°C.';
    return;
  }
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById(displayId).innerText = 'Temperatura actualizada: ' + value.toFixed(2) + '°C';
    }
  };
  xhttp.open('GET', endpoint + '?temp=' + value.toFixed(2), true);
  xhttp.send();
}
</script>

<h2>Control del Rele</h2>
<p id='relayMessage'>Cargando estado...</p>
<button id='relayButton' onclick='toggleRelay()'>Encender Rele</button>

<h2>Lecturas de Sensores</h2>
<p>MLX90614 - Temp Objeto: <span id='mlxTempObj'>--°C</span>, Temp Ambiente: <span id='mlxTempAmb'>--°C</span></p>
<p>AHT10 Interno - Temp: <span id='ahtTemp1'>--°C</span>, Humedad: <span id='ahtHum1'>--%</span></p>
<p>DHT11 Interno - Temp: <span id='dhtTemp2'>--°C</span>, Humedad: <span id='dhtHum2'>--%</span></p>
<p>AHT10 Externo - Temp: <span id='ahtTemp2'>--°C</span>, Humedad: <span id='ahtHum2'>--%</span></p>
<p>DHT11 Externo - Temp: <span id='dhtTemp1'>--°C</span>, Humedad: <span id='dhtHum1'>--%</span></p>

<button onclick='exportCSV()'>Exportar a CSV</button>

<div id='plotlyChartContainer' style='display: flex; justify-content: center; align-items: center; width: 100%;'>
  <div id='plotlyChart' style='width: 100%; max-width: 900px; height: 600px;'></div>
</div>
</div>

<script>
let csvData = [['Tiempo', 'AHT10 Interno', 'AHT10 Externo', 'DHT11 Interno', 'DHT11 Externo', 'MLX90614 Objeto', 'MLX90614 Ambiente']];

let lastTime = '';
function updatePlotlyChart() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      var data = JSON.parse(this.responseText);
      if (data.tiempo !== lastTime) {
        csvData.push([data.tiempo, data.ahtTemp1, data.ahtTemp2, data.dhtTemp2, data.dhtTemp1, data.mlxTempObj, data.mlxTempAmb]);
        lastTime = data.tiempo;
        var update = {
          x: [[data.tiempo], [data.tiempo], [data.tiempo], [data.tiempo], [data.tiempo], [data.tiempo]],
          y: [[data.ahtTemp1], [data.ahtTemp2], [data.dhtTemp2], [data.dhtTemp1], [data.mlxTempObj], [data.mlxTempAmb]]
        };
        Plotly.extendTraces('plotlyChart', update, [0, 1, 2, 3, 4, 5]);
      }
    }
  };
  xhttp.open('GET', '/sensordata', true);
  xhttp.send();
}
setInterval(updatePlotlyChart, 1000);

function exportCSV() {
  let csvContent = 'data:text/csv;charset=utf-8,' + csvData.map(e => e.join(',')).join('\\n');
  var link = document.createElement('a');
  link.setAttribute('href', encodeURI(csvContent));
  link.setAttribute('download', 'Heat_Transfer.csv');
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
}
</script>

<script>
Plotly.newPlot('plotlyChart', [{
  x: [], y: [], mode: 'lines', name: 'AHT10 Interno', line: {color: 'red'}
}, {
  x: [], y: [], mode: 'lines', name: 'AHT10 Externo', line: {color: 'blue'}
}, {
  x: [], y: [], mode: 'lines', name: 'DHT11 Interno', line: {color: 'purple'}
}, {
  x: [], y: [], mode: 'lines', name: 'DHT11 Externo', line: {color: 'orange'}
}, {
  x: [], y: [], mode: 'lines', name: 'MLX90614 Objeto', line: {color: 'green'}
}, {
  x: [], y: [], mode: 'lines', name: 'MLX90614 Ambiente', line: {color: 'brown'}
}], {
  title: 'Grafica de Temperaturas de Sensores',
  xaxis: { title: 'Tiempo' },
  yaxis: { title: 'Temperatura (°C)' }
}, {locale: 'es', displaylogo: false, responsive: true, displayModeBar: true});
</script>

<div id='csvInterface' style='display: none;'>
<h2>Importar CSV y Graficar</h2>
<input type='file' id='importCSVInput' accept='.csv' onchange='importCSV(event)'>
<div id='csvPlotlyChart' style='width: 90%; height: 500px; margin: auto;'></div>

<script>
function importCSV(event) {
  var file = event.target.files[0];
  if (file) {
    var reader = new FileReader();
    reader.onload = function(e) {
      var rows = e.target.result.split('\\n').slice(1);
      var x = [], yAHT1 = [], yAHT2 = [], yDHT2 = [], yDHT1 = [], yObj = [], yAmb = [];
      rows.forEach(row => {
        var cols = row.split(',');
        if (cols.length >= 7) {
          x.push(cols[0]);
          yAHT1.push(parseFloat(cols[1]));
          yAHT2.push(parseFloat(cols[2]));
          yDHT2.push(parseFloat(cols[3]));
          yDHT1.push(parseFloat(cols[4]));
          yObj.push(parseFloat(cols[5]));
          yAmb.push(parseFloat(cols[6]));
        }
      });
      Plotly.newPlot('csvPlotlyChart', [
        { x: x, y: yAHT1, mode: 'lines', name: 'AHT10 Interno', line: {color: 'red'} },
        { x: x, y: yAHT2, mode: 'lines', name: 'AHT10 Externo', line: {color: 'blue'} },
        { x: x, y: yDHT2, mode: 'lines', name: 'DHT11 Interno', line: {color: 'purple'} },
        { x: x, y: yDHT1, mode: 'lines', name: 'DHT11 Externo', line: {color: 'orange'} },
        { x: x, y: yObj, mode: 'lines', name: 'MLX90614 Objeto', line: {color: 'green'} },
        { x: x, y: yAmb, mode: 'lines', name: 'MLX90614 Ambiente', line: {color: 'brown'} }
      ], { title: 'Grafica desde CSV', xaxis: { title: 'Tiempo' }, yaxis: { title: 'Temperatura (°C)' } }, {locale: 'es', displaylogo: false, responsive: true, displayModeBar: true});
    };
    reader.readAsText(file);
  }
}

function showExperiment() {
  document.getElementById('experimentInterface').style.display = 'block';
  document.getElementById('csvInterface').style.display = 'none';
}

function showCSVGraph() {
  document.getElementById('experimentInterface').style.display = 'none';
  document.getElementById('csvInterface').style.display = 'block';
}
</script>
</div>
</body>
</html>
)HTML";


