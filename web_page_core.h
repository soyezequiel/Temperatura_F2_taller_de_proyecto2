// web_page_core.h
#pragma once
#include <Arduino.h>

const char index_head[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang='es'>
<head>
  <script>const UPDATE_MS = %UPDATE_MS%;</script>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <!-- Plotly solo si hay gráfica (lo inyectamos desde web_chart.h) -->
  <style>
    body {font-family: Arial; text-align: center;}
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

  <!--CHART_PLACEHOLDER-->
)HTML";

const char index_tail[] PROGMEM = R"HTML(
  <script>
  // ===== Utilidades genéricas =====
  function setText(id, txt) { var el = document.getElementById(id); if (el) el.innerHTML = txt; }
  function n(x){ var v = parseFloat(x); return isNaN(v) ? null : v; }
  // ===== AJAX helpers =====
  function callEndpoint(url, cb) {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.onreadystatechange = function () {
      if (xhr.readyState === 4 && xhr.status === 200 && cb) cb();
    };
    xhr.send();
  }
  // ===== Estado/acciones UI =====
  function toggleRelay() {
    callEndpoint('/toggleRelay', function(){ updateSensorData(); });
  }

  function bindForms() {
    var f1 = document.getElementById('setLimitForm');
    if (f1) f1.addEventListener('submit', function (e) {
      e.preventDefault();
      var value = parseFloat(document.getElementById('setLimitInput').value);
      if (isNaN(value) || value < 0 || value > 90) { setText('tempLimitMessage', 'El valor debe estar entre 0 y 90°C.'); return; }
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
      if (isNaN(value) || value < 0 || value > 45) { setText('criticalTempLimitMessage', 'El valor debe estar entre 0 y 45°C.'); return; }
      callEndpoint('/setcriticallimit/?temp=' + value.toFixed(2), function () {
        setText('criticalTempLimitMessage', '');
        setText('criticalTempLimitDisplay', value.toFixed(2) + '°C');
        updateSensorData();
      });
    });
  }

  // ===== Sensores (DOM básico; si hay gráfica, su JS la usa también) =====
  function updateSensorData() {
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function () {
      if (this.readyState === 4 && this.status === 200) {
        var d = JSON.parse(this.responseText);
        setText('ahtTemp1', d.ahtTemp1 + '°C'); setText('ahtHum1',  d.ahtHum1  + '%');
        setText('ahtTemp2', d.ahtTemp2 + '°C'); setText('ahtHum2',  d.ahtHum2  + '%');
        setText('dhtTemp1', d.dhtTemp1 + '°C'); setText('dhtHum1',  d.dhtHum1  + '%');
        setText('dhtTemp2', d.dhtTemp2 + '°C'); setText('dhtHum2',  d.dhtHum2  + '%');
        setText('mlxTempObj', d.mlxTempObj + '°C'); setText('mlxTempAmb', d.mlxTempAmb + '°C');
        setText('relayMessage', d.relayMessage);
        setText('relayButton', d.relayState ? 'Apagar Rele' : 'Encender Rele');
        csvPush(d);
        // Si la gráfica está habilitada, el script de la gráfica se “cuelga” de esta función:
        if (window.chartUpdate) window.chartUpdate(d);
      }
    };
    xhr.open('GET', '/sensordata', true);
    xhr.send();
  }

  document.addEventListener('DOMContentLoaded', function(){
    bindForms();
    updateSensorData();
    setInterval(updateSensorData, UPDATE_MS);
  });
    // ===== CSV =====
    let csvData = [['Tiempo','AHT10 Interno','AHT10 Externo','DHT11 Interno','DHT11 Externo','MLX Obj','MLX Amb']];
    let lastCsvTime = '';
    function csvPush(d){
      // Evita duplicar la misma marca de tiempo
      if (!d || !d.tiempo || d.tiempo === lastCsvTime) return;
      lastCsvTime = d.tiempo;
      csvData.push([
        d.tiempo,
        d.ahtTemp1, d.ahtTemp2,
        d.dhtTemp2, d.dhtTemp1,
        d.mlxTempObj, d.mlxTempAmb
      ]);
    }
    function exportCSV(){
      // Arma el CSV en el navegador
      const rows = csvData.map(row => row.join(',')).join('\n');
      const blob = new Blob([rows], {type: 'text/csv;charset=utf-8;'});
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url; a.download = 'Heat_Transfer.csv';
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    }
  </script>
</body>
</html>
)HTML";
