#pragma once
#include <Arduino.h>

const char index_html[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang='es'>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <script src='https://cdn.plot.ly/plotly-latest.min.js'></script>
  <script src='https://cdn.plot.ly/plotly-locale-es-latest.js'></script>
  <style>
    body {font-family: Arial; text-align: center;}
    h1 {color:#333;}
    button {margin:6px; padding:8px 14px;}
  </style>
</head>
<body>
  <h1>Heat Transfer - Monitor y Control de Rele</h1>

  <h2>Configuración de Límites</h2>
  <p>Temperatura de calentamiento (MLX90614): <span id="tempLimitDisplay">No configurado</span></p>
  <form id='setLimitForm'>
    <input type='number' id='setLimitInput' min='0' max='90' step='0.01' placeholder='0.00 - 90.00 °C' required>
    <input type='submit' value='Actualizar Límite'>
  </form>
  <p id='tempLimitMessage' style='color: red;'></p>

  <p>Temperatura crítica (AHT10): <span id="criticalTempLimitDisplay">No configurado</span></p>
  <form id='setCriticalLimitForm'>
    <input type='number' id='setCriticalLimitInput' min='0' max='45' step='0.01' placeholder='0.00 - 45.00 °C' required>
    <input type='submit' value='Actualizar Límite Crítico'>
  </form>
  <p id='criticalTempLimitMessage' style='color: red;'></p>

  <h2>Control del Relé</h2>
  <p id='relayMessage'>Cargando estado...</p>
  <button id='relayButton' onclick='toggleRelay()'>Encender Rele</button>

  <h2>Lecturas de Sensores</h2>
  <p>MLX90614 - Objeto: <span id='mlxTempObj'>--°C</span> | Ambiente: <span id='mlxTempAmb'>--°C</span></p>
  <p>AHT10 Interno: <span id='ahtTemp1'>--°C</span> / <span id='ahtHum1'>--%</span></p>
  <p>AHT10 Externo: <span id='ahtTemp2'>--°C</span> / <span id='ahtHum2'>--%</span></p>
  <p>DHT11 Interno: <span id='dhtTemp2'>--°C</span> / <span id='dhtHum2'>--%</span></p>
  <p>DHT11 Externo: <span id='dhtTemp1'>--°C</span> / <span id='dhtHum1'>--%</span></p>

  <button onclick='exportCSV()'>Exportar CSV</button>

  <div id='plotlyChartContainer' style='display:flex;justify-content:center;align-items:center;width:100%;'>
    <div id='plotlyChart' style='width:100%;max-width:900px;height:600px;'></div>
  </div>

  <script>
// === Estado de gráfico ===
let lastTime = '';
let chartReady = false;

// === Init al cargar ===
document.addEventListener('DOMContentLoaded', function () {
  // Plot vacío con 6 trazas
  Plotly.newPlot('plotlyChart', [
    { x: [], y: [], mode: 'lines', name: 'AHT10 Interno' },
    { x: [], y: [], mode: 'lines', name: 'AHT10 Externo' },
    { x: [], y: [], mode: 'lines', name: 'DHT11 Interno' },
    { x: [], y: [], mode: 'lines', name: 'DHT11 Externo' },
    { x: [], y: [], mode: 'lines', name: 'MLX Obj'      },
    { x: [], y: [], mode: 'lines', name: 'MLX Amb'      }
  ], {
    title: 'Gráfica de Temperaturas',
    xaxis: { title: 'Tiempo' },
    yaxis: { title: '°C' }
  }, { locale: 'es', displaylogo: false, responsive: true, displayModeBar: true });

  chartReady = true;

  // Handlers de formularios
  bindForms();

  // Refresco cada 1s
  updateSensorData();
  setInterval(updateSensorData, 1000);
});

// === Lectura de sensores + actualización de DOM + gráfico ===
function updateSensorData() {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function () {
    if (this.readyState === 4 && this.status === 200) {
      var data = JSON.parse(this.responseText);

      // DOM
      setText('ahtTemp1',  data.ahtTemp1 + '°C');
      setText('ahtHum1',   data.ahtHum1  + '%');
      setText('ahtTemp2',  data.ahtTemp2 + '°C');
      setText('ahtHum2',   data.ahtHum2  + '%');
      setText('dhtTemp1',  data.dhtTemp1 + '°C');
      setText('dhtHum1',   data.dhtHum1  + '%');
      setText('dhtTemp2',  data.dhtTemp2 + '°C');
      setText('dhtHum2',   data.dhtHum2  + '%');
      setText('mlxTempObj',data.mlxTempObj + '°C');
      setText('mlxTempAmb',data.mlxTempAmb + '°C');
      setText('relayMessage', data.relayMessage);
      setText('relayButton', data.relayState ? 'Apagar Rele' : 'Encender Rele');

      // Si querés mostrar en la página los límites actuales, podés enviarlos
      // también en /sensordata desde el ESP32 y descomentar esto:
      // setText('tempLimitDisplay', (data.tempLimit || 'No configurado'));
      // setText('criticalTempLimitDisplay', (data.criticalTempLimit || 'No configurado'));

      // Gráfico (solo si hay nuevo timestamp)
      if (chartReady && data.tiempo && data.tiempo !== lastTime) {
        lastTime = data.tiempo;
        Plotly.extendTraces('plotlyChart', {
          x: [[data.tiempo],[data.tiempo],[data.tiempo],[data.tiempo],[data.tiempo],[data.tiempo]],
          y: [[n(data.ahtTemp1)],[n(data.ahtTemp2)],[n(data.dhtTemp2)],[n(data.dhtTemp1)],[n(data.mlxTempObj)],[n(data.mlxTempAmb)]]
        }, [0,1,2,3,4,5]);
      }
    }
  };
  xhr.open('GET', '/sensordata', true);
  xhr.send();
}

// === Toggle relé ===
function toggleRelay() {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', '/toggleRelay', true);
  xhr.onreadystatechange = function () {
    if (xhr.readyState === 4) {
      // Relee estado
      updateSensorData();
    }
  };
  xhr.send();
}

// === Formularios de límites (AJAX GET a tus endpoints) ===
function bindForms() {
  var f1 = document.getElementById('setLimitForm');
  if (f1) f1.addEventListener('submit', function (e) {
    e.preventDefault();
    var value = parseFloat(document.getElementById('setLimitInput').value);
    if (isNaN(value) || value < 0 || value > 90) {
      setText('tempLimitMessage', 'El valor debe estar entre 0 y 90°C.');
      return;
    }
    callEndpoint('/setlimit/?temp=' + value.toFixed(2), function () {
      setText('tempLimitMessage', '');
      setText('tempLimitDisplay', value.toFixed(2) + '°C');
      updateSensorData();
    });
  });

  var f2 = document.getElementById('setCriticalLimitForm');
  if (f2) f2.addEventListener('submit', function (e) {
    e.preventDefault();
    var value = parseFloat(document.getElementById('setCriticalLimitInput').value);
    if (isNaN(value) || value < 0 || value > 45) {
      setText('criticalTempLimitMessage', 'El valor debe estar entre 0 y 45°C.');
      return;
    }
    callEndpoint('/setcriticallimit/?temp=' + value.toFixed(2), function () {
      setText('criticalTempLimitMessage', '');
      setText('criticalTempLimitDisplay', value.toFixed(2) + '°C');
      updateSensorData();
    });
  });
}

// === Utilidades ===
function setText(id, txt) {
  var el = document.getElementById(id);
  if (el) el.innerHTML = txt;
}
function callEndpoint(url, cb) {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', url, true);
  xhr.onreadystatechange = function () {
    if (xhr.readyState === 4 && xhr.status === 200 && cb) cb();
  };
  xhr.send();
}
function n(x){ var v = parseFloat(x); return isNaN(v) ? null : v; }
</script>


</body>
</html>
)HTML";
