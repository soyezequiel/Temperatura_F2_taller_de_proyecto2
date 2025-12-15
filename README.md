# Monitor de temperatura y rele en ESP32

Proyecto Arduino para ESP32 (ESP32-WROOM-32 DevKit) que lee un MLX90614, dos AHT10 y dos DHT11, publica una pagina web (modo LAN, sin salir a internet) y permite encender o apagar un rele siguiendo limites de temperatura configurables. Informa el estado (relayRequested, relayLocked, relayCutByTemp) y exporta CSV. Lo siguiente resume la guia de instalacion pedida en la bitacora.

## Dependencias (incluyelas en tu .zip de entrega)
- Arduino IDE 2.x
- Paquete de tarjetas: **esp32 by Espressif** (Board Manager)
- Librerias (Gestor de bibliotecas): **Adafruit MLX90614 Library**, **Adafruit AHTX0**, **Adafruit Unified Sensor**, **DHT sensor library** (instala Adafruit BusIO). `WiFi.h` y `Wire.h` vienen con el core ESP32.

## Hardware probado (segun bitacora)
- Placa: ESP32-WROOM-32 DevKit (NodeMCU ESP32).
- Rele 5 V activo en alto (bobina a 5 V, control a 3.3 V desde GPIO 15).
- Resistencias ceramicas a 12 V para carga; sensores y logica a 3.3 V; conectores DC rotulados 3.3 V y 12 V en la caja IP65.
- Dos buses I2C: Wire (SDA 21, SCL 22) y TwoWire I2C_2 (SDA 18, SCL 19).

## Cableado por defecto (pins en `temperaturaF2EA.ino`)
- I2C_1: SDA 21, SCL 22 -> MLX90614 + AHT10 #1
- I2C_2: SDA 18, SCL 19 -> AHT10 #2 (usa `TwoWire I2C_2(1)`)
- DHT11 externo: GPIO 13
- DHT11 interno: GPIO 14
- Rele (activo en HIGH): GPIO 15
- LED de estado: GPIO 4
- Alimentacion: 3V3 y GND comunes

## Guia paso a paso (IDE limpio)
1) Descarga el repo (Code > Download ZIP) y descomprime.  
2) Instala Arduino IDE 2.x.  
3) En IDE: Archivo > Preferencias, agrega la URL de ESP32 `https://dl.espressif.com/dl/package_esp32_index.json`.  
4) Driver USB-UART (si el puerto no aparece):  
   - La mayoria de DevKit trae **CP210x** (Silabs) o **CH340** (WCH).  
   - Windows/macOS: instala el driver correspondiente desde el fabricante (Silicon Labs CP210x o CH340). En Linux suele venir incluido.  
   - CP210x Windows: https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers?tab=downloads  
   - Re-conecta la placa y verifica que aparezca el puerto serie.  
5) Herramientas > Placa > Gestor de tarjetas: instala **esp32 by Espressif Systems**. Selecciona `ESP32 Dev Module` o la variante DevKit que uses.  
6) Herramientas > Administrar bibliotecas: instala las librerias listadas en Dependencias.  
6) Abre `temperaturaF2EA.ino`. Configura tu WiFi cerca del inicio:
   ```cpp
   const char* ssid     = "tu_ssid";
   const char* password = "tu_password";
   ```
7) Opcional: desactiva la grafica poniendo `#define ENABLE_CHART 0` para ahorrar memoria.  
8) Conecta el hardware segun el cableado por defecto. Alimenta sensores a 3.3 V, rele a 5 V, carga a 12 V.  
9) Herramientas: elige puerto serie, velocidad de carga por defecto, y sube el sketch. Monitor Serie a 115200 bps para ver la IP.  
10) En un navegador de la misma LAN abre `http://<ip_del_esp32>` para la interfaz web.

## Uso y comportamiento (probado en bitacora)
- El rele arranca apagado. Solo se habilita cuando configuras ambos limites (normal MLX y critico AHT).  
- Corte por temperatura MLX usa histéresis de 5 °C (`RELAY_VENTANA`): corta al superar el limite, reenciende al bajar 5 °C si no hay bloqueo.  
- Corte critico por AHT bloquea el rele (relayLocked=true) hasta reconfigurar.  
- Boton muestra el estado solicitado (relayRequested) aunque el rele este cortado por temperatura.  
- Exportar CSV entrega las lecturas actuales; `/sensordata` sirve JSON para integrar con otros sistemas.

### Valores de prueba recomendados (bitacora)
- Prueba de control rapido con carga: MLX=70 °C, AHT interno=40 °C para ver el ciclo de corte/encendido en 20-40 minutos.  
- Prueba extendida con tacho y resistencias: MLX=90 °C, AHT interno=40 °C (usado en las corridas con balde y resistencias ceramicas).

## Pruebas rapidas
- Sin carga: configura limites y verifica que el boton de rele responde y que `relayLocked`/`relayCutByTemp` cambian al cruzar limites simulando con calor suave.  
- Con carga: monitorea en el grafico (o JSON) el serrucho de temperaturas; verifica que el rele corta a la subida y reenciende tras bajar ~5 °C.  
- Si habilitas `ENABLE_CHART 1`, confirma que la pagina carga Plotly (puede tardar un poco mas).

## Solucion de problemas
- No conecta a WiFi: revisa SSID/clave y que la red sea 2.4 GHz.  
- Sensores sin lectura: verifica 3.3 V y el bus I2C correcto; DHT en GPIO 13/14 con resistencia pull-up adecuada.  
- Rele no enciende: asegúrate de haber configurado ambos limites; revisa `relayLocked` (critico) y `relayCutByTemp` (histéresis MLX).  
- Pagina sin grafica: deja `ENABLE_CHART 0` si hay falta de memoria o latencia alta.
