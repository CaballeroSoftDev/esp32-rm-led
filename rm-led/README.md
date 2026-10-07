# RM LED - ESP32-S3

Proyecto de aprendizaje con ESP-IDF para controlar LEDs mediante Wi-Fi y MQTT.

## Funciones

- Controlar el WS2812 integrado de la placa con valores RGB.
- Encender y apagar un LED externo de dos patas.
- Publicar el estado MQTT del dispositivo (`online` / `offline`).

## Topics MQTT

El client ID y la raíz de los topics son `rm-led-esp32s3`.

| Topic | Dirección | Payload |
|---|---|---|
| `rm-led-esp32s3/cmd/led/ws2812/set` | Backend → ESP32 | `R,G,B` (cada valor de 0 a 255), por ejemplo `255,23,21` |
| `rm-led-esp32s3/cmd/led/simple/set` | Backend → ESP32 | `1` para encender; `0` para apagar |
| `rm-led-esp32s3/availability` | ESP32/broker → backend | `online` / `offline` |

El estado usa mensajes retenidos con QoS 1. El ESP32 publica `online` al conectarse; el broker publica `offline` mediante LWT si detecta una desconexión inesperada.

En EMQX, el usuario del ESP32 necesita permiso para suscribirse a los dos topics de comandos y publicar en `rm-led-esp32s3/availability`.

## Conexión del LED externo

- Señal en **GPIO10**.
- Conecta GPIO10 al ánodo del LED a través de una resistencia (por ejemplo, 220–330 Ω).
- Conecta el cátodo del LED a GND.
- El LED integrado WS2812 usa **GPIO48**.

Ambos LEDs se inicializan apagados.

## Configuración

Antes de compilar, configura los valores locales de Wi-Fi en `main/services/connectivity/wifi_manager.c` y los de MQTT en `main/services/connectivity/mqtt_manager.c` (broker, usuario y contraseña).

El cliente usa MQTT sobre TLS en el puerto 8883. En EMQX, configura el client ID `rm-led-esp32s3` y los permisos descritos arriba.

## Compilar y cargar

Requiere ESP-IDF 6.1 configurado para ESP32-S3. Activa el entorno de ESP-IDF y ejecuta desde la raíz del proyecto:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

Reemplaza `PORT` por el puerto serie de la placa, por ejemplo `/dev/ttyUSB0`.

## Organización del código

```text
main/
├── app/                 # Enrutamiento de comandos MQTT
├── devices/led/         # Drivers del WS2812 y LED GPIO
├── services/connectivity/ # Gestores de Wi-Fi y MQTT
└── main.c               # Inicialización de la aplicación
```
