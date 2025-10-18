// web_chart.h
#pragma once
#include <Arduino.h>

// Bloque completo de la gráfica (HTML contenedor + scripts Plotly + lógica)
const char chart_block[] PROGMEM = R"HTML(
  <!-- Plotly (solo si hay gráfica) -->
  <script src='https://cdn.plot.ly/plotly-latest.min.js'></script>
  <script src='https://cdn.plot.ly/plotly-locale-es-latest.js'></script>

  <div id='plotlyChartContainer' style='display:flex;justify-content:center;align-items:center;width:100%;'>
    <div id='plotlyChart' style='width:100%;max-width:900px;height:600px;'></div>
  </div>

  <script>
  // Se engancha a la app base usando window.chartUpdate
  let lastTime = '';
  let chartReady = false;

  document.addEventListener('DOMContentLoaded', function(){
    Plotly.newPlot('plotlyChart', [
      { x:[], y:[], mode:'lines', name:'AHT10 Interno'  },
      { x:[], y:[], mode:'lines', name:'AHT10 Externo'  },
      { x:[], y:[], mode:'lines', name:'DHT11 Interno'  },
      { x:[], y:[], mode:'lines', name:'DHT11 Externo'  },
      { x:[], y:[], mode:'lines', name:'MLX Obj'        },
      { x:[], y:[], mode:'lines', name:'MLX Amb'        }
    ], { title:'Gráfica de Temperaturas', xaxis:{title:'Tiempo'}, yaxis:{title:'°C'} },
       { locale:'es', displaylogo:false, responsive:true, displayModeBar:true });
    chartReady = true;
  });

  // La app base llama a window.chartUpdate(d) desde updateSensorData()
  window.chartUpdate = function(d){
    if (!chartReady || !d || !d.tiempo || d.tiempo === lastTime) return;
    lastTime = d.tiempo;
    Plotly.extendTraces('plotlyChart', {
      x:[[d.tiempo],[d.tiempo],[d.tiempo],[d.tiempo],[d.tiempo],[d.tiempo]],
      y:[[parseFloat(d.ahtTemp1)],[parseFloat(d.ahtTemp2)],[parseFloat(d.dhtTemp2)],[parseFloat(d.dhtTemp1)],[parseFloat(d.mlxTempObj)],[parseFloat(d.mlxTempAmb)]]
    }, [0,1,2,3,4,5]);
  };
  </script>
)HTML";
