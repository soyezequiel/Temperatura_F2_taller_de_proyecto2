# Monitor de temperatura y rele en ESP32

Proyecto Arduino para ESP32 que lee un MLX90614, dos AHT10 y dos DHT11, publica una pagina web con graficas (opcional) y permite encender o apagar un rele siguiendo limites de temperatura configurables.

## Requisitos
- Placa ESP32 (probado con ESP32 DevKit v1).
- Sensores: 1x MLX90614 (I2C), 2x AHT10 (I2C), 2x DHT11, 1x rele activo en alto, LED en pin de estado.
- Arduino IDE 2.x con el paquete **esp32** de Espressif instalado desde el Gestor de tarjetas.
- Librerias desde el Gestor de librerias: **Adafruit MLX90614 Library**, **Adafruit AHTX0**, **Adafruit Unified Sensor**, **DHT sensor library** (instala Adafruit BusIO como dependencia). `WiFi.h` y `Wire.h` vienen con el core de ESP32.

## Cableado por defecto (pins en `temperaturaF2EA.ino`)
- I2C_1: SDA 21, SCL 22 -> MLX90614 + AHT10 #1
- I2C_2: SDA 18, SCL 19 -> AHT10 #2 (usa `TwoWire I2C_2(1)`)
- DHT11 externo: GPIO 13
- DHT11 interno: GPIO 14
- Rele (activo en HIGH): GPIO 15
- LED de estado: GPIO 4
- Alimentacion: 3V3 y GND comunes para todos los modulos

## Instalacion rapida
1) Clona o descarga este repositorio y abre `temperaturaF2EA.ino` en Arduino IDE.  
2) Herramientas > Placa > Gestor de tarjetas: instala **esp32** de Espressif y selecciona `ESP32 Dev Module`.  
3) Librerias: instala las librerias listadas arriba desde Herramientas > Administrar bibliotecas.  
4) Configura tu WiFi editando cerca del inicio de `temperaturaF2EA.ino`:
   ```cpp
   const char* ssid     = "tu_ssid";
   const char* password = "tu_password";
   ```
5) Opcional: para reducir uso de flash/RAM desactiva la grafica poniendo `#define ENABLE_CHART 0` en el mismo archivo.  
6) Conecta el ESP32 por USB, en Herramientas elige el puerto correcto y sube el sketch. Velocidad serie: 115200 bps.

## Uso
- Abre el Monitor Serie a 115200. Al iniciar veras la IP asignada por tu red WiFi.  
- En un navegador (mismo segmento de red) entra a `http://<ip_del_esp32>` para cargar la interfaz:
  - Ajusta el limite de temperatura (MLX90614) y el limite critico (AHT10). Ambos deben estar definidos para habilitar el rele.
  - Boton para encender/apagar el rele; si se supera el limite o el critico se bloquea automaticamente.
  - Lecturas en vivo de todos los sensores y, si `ENABLE_CHART` esta en 1, grafica de historico.
  - Boton **Exportar CSV** para descargar las lecturas actuales.

## Solucion de problemas
- No conecta a WiFi: verifica SSID/clave y que sea red 2.4 GHz.  
- Sin lecturas en un sensor: revisa alimentacion 3V3/GND y los pines I2C o de datos segun el listado.
