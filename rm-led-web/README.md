# RM LED Web - Panel de Control IoT

Panel de control web en Angular para monitorear y controlar en tiempo real los LEDs del microcontrolador ESP32-S3 mediante MQTT sobre WebSockets seguros (WSS).

## Funciones

- Controlar el LED WS2812 integrado (GPIO 48) con selector de espectro 2D, barra HUE arcoíris, campos numéricos RGB, selector nativo del sistema y 8 presets rápidos.
- Encender y apagar el LED externo (GPIO 10) con interruptor deslizante y botones directos.
- Monitorear en tiempo real la conexión del navegador al broker MQTT y la disponibilidad del ESP32-S3 (`online` / `offline`).
- Transmisión fluida con *throttling* (~60 ms) durante el arrastre de color para evitar saturación de red.
- Configuración dinámica de credenciales almacenada localmente en el navegador (`localStorage`), sin secretos expuestos en el código fuente.

## Topics MQTT

El identificador de cliente del firmware y la raíz de los topics son `rm-led-esp32s3`. La aplicación web genera un client ID dinámico con prefijo `rm-led-web-`.

| Topic | Dirección | Payload | Descripción |
|---|---|---|---|
| `rm-led-esp32s3/cmd/led/ws2812/set` | Web → ESP32 | `R,G,B` (0 a 255 cada valor), por ejemplo `255,55,72` | Fija el color del LED WS2812 |
| `rm-led-esp32s3/cmd/led/simple/set` | Web → ESP32 | `1` (encender) o `0` (apagar) | Conmuta el LED de GPIO10 |
| `rm-led-esp32s3/availability` | ESP32/broker → Web | `online` / `offline` | Estado de presencia del ESP32-S3 |

La disponibilidad utiliza mensajes retenidos con QoS 1. El ESP32 publica `online` al conectarse; el broker EMQX publica `offline` mediante LWT si detecta una desconexión inesperada.

En EMQX, el usuario configurado para la web necesita permisos de publicación en `rm-led-esp32s3/cmd/led/#` y de suscripción en `rm-led-esp32s3/availability`.

## Conexión y Requisitos de Red

Los navegadores web no pueden abrir sockets TCP directos (puerto 8883), por lo que la aplicación se conecta mediante **MQTT sobre WebSockets seguros**:

- **Protocolo y puerto:** WSS en el puerto **8084** de EMQX.
- **Ruta del WebSocket:** `wss://<BROKER_HOST>:8084/mqtt`
- **Certificados TLS:** Validados de forma nativa por el navegador.

## Configuración

Al abrir la aplicación por primera vez (o mediante el botón **Config** en la barra superior), puedes ingresar los datos de conexión con tu broker EMQX:

- **WebSocket URL:** `wss://<tu-host-de-emqx>:8084/mqtt`
- **Username:** Usuario asignado en EMQX (por ejemplo, `web-pub`).
- **Password:** Contraseña del usuario.

Estos valores se almacenan únicamente en el `localStorage` de tu navegador, garantizando que el repositorio Git y los paquetes compilados no contengan credenciales sensibles.

## Ejecutar y Compilar

Requiere **Node.js** (versión 20 o superior). Desde la carpeta `rm-led-web`:

1. Instalar dependencias:
   ```bash
   npm install
   ```

2. Iniciar servidor de desarrollo local:
   ```bash
   npm start
   ```
   Abre [http://localhost:4200](http://localhost:4200) en el navegador.

3. Compilar para producción:
   ```bash
   npm run build
   ```
   Los artefactos optimizados se generarán en la carpeta `dist/rm-led-web`.

## Organización del código

```text
src/
├── app/
│   ├── services/
│   │   └── mqtt.service.ts   # Conexión WSS, topics, throttling y reactividad (Signals)
│   ├── app.ts                # Componente principal, estado y sincronización de color
│   ├── app.html              # Plantilla del panel, animaciones, visualizadores y modal
│   └── app.css               # Estilos personalizados, efectos glow y sliders
├── styles.css                # Configuración base de Tailwind CSS v4 y tema oscuro
└── index.html                # Entrada HTML y fuentes (Inter, JetBrains Mono)
```
