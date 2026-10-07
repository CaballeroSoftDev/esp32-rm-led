import { Injectable, signal } from '@angular/core';
import mqtt, { MqttClient } from 'mqtt';

export interface MqttConfig {
  brokerUrl: string;
  username: string;
  password: string;
  clientId: string;
}

export const DEFAULT_MQTT_CONFIG: MqttConfig = {
  brokerUrl: '',
  username: '',
  password: '',
  clientId: '',
};

const TOPIC_ROOT = 'rm-led-esp32s3';
export const MQTT_TOPICS = {
  CMD_WS2812_SET: `${TOPIC_ROOT}/cmd/led/ws2812/set`,
  CMD_SIMPLE_SET: `${TOPIC_ROOT}/cmd/led/simple/set`,
  AVAILABILITY: `${TOPIC_ROOT}/availability`,
};

const STORAGE_KEY = 'rm_led_mqtt_config';

@Injectable({
  providedIn: 'root',
})
export class MqttService {
  private client: MqttClient | null = null;
  private throttleTimer: any = null;
  private pendingRgbPayload: string | null = null;

  // Reactive state signals
  readonly isConnected = signal(false);
  readonly esp32Status = signal<'online' | 'offline' | 'unknown'>('unknown');
  readonly lastError = signal<string | null>(null);
  readonly lastActivity = signal<{ topic: string; payload: string; time: string } | null>(null);
  readonly currentConfig = signal<MqttConfig>(this.loadConfig());

  constructor() {
    // Auto-connect on startup with saved or default configuration
    this.connect();
  }

  loadConfig(): MqttConfig {
    try {
      const stored = localStorage.getItem(STORAGE_KEY);
      if (stored) {
        const parsed = JSON.parse(stored);
        return {
          ...DEFAULT_MQTT_CONFIG,
          ...parsed,
          clientId: parsed.clientId || this.generateClientId(),
        };
      }
    } catch (e) {
      console.warn('Could not read saved MQTT config, using defaults', e);
    }
    return { ...DEFAULT_MQTT_CONFIG, clientId: this.generateClientId() };
  }

  private generateClientId(): string {
    return 'rm-led-web-' + Math.random().toString(16).substring(2, 8);
  }

  saveConfig(config: MqttConfig): void {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(config));
      this.currentConfig.set(config);
    } catch (e) {
      console.warn('Could not save MQTT config', e);
    }
  }

  connect(customConfig?: MqttConfig): void {
    if (this.client) {
      this.disconnect();
    }

    const config = customConfig || this.currentConfig();
    if (!config.brokerUrl) {
      // No broker configured yet; wait for user input in Config modal
      this.isConnected.set(false);
      return;
    }

    this.saveConfig(config);
    this.lastError.set(null);

    try {
      this.client = mqtt.connect(config.brokerUrl, {
        clientId: config.clientId,
        username: config.username,
        password: config.password,
        clean: true,
        connectTimeout: 8000,
        reconnectPeriod: 5000,
      });

      this.client.on('connect', () => {
        this.isConnected.set(true);
        this.lastError.set(null);

        // Subscribe to device availability topic
        this.client?.subscribe(MQTT_TOPICS.AVAILABILITY, { qos: 1 }, (err) => {
          if (err) {
            console.error('Subscription error:', err);
          }
        });
      });

      this.client.on('message', (topic, message) => {
        const payload = message.toString();
        this.lastActivity.set({
          topic,
          payload,
          time: new Date().toLocaleTimeString(),
        });

        if (topic === MQTT_TOPICS.AVAILABILITY) {
          if (payload === 'online') {
            this.esp32Status.set('online');
          } else if (payload === 'offline') {
            this.esp32Status.set('offline');
          }
        }
      });

      this.client.on('error', (err) => {
        console.error('MQTT error:', err);
        this.lastError.set(err.message || 'Connection error');
      });

      this.client.on('close', () => {
        this.isConnected.set(false);
      });

      this.client.on('offline', () => {
        this.isConnected.set(false);
      });
    } catch (err: any) {
      console.error('MQTT connect exception:', err);
      this.lastError.set(err.message || 'Error connecting to MQTT');
      this.isConnected.set(false);
    }
  }

  disconnect(): void {
    if (this.client) {
      try {
        this.client.end(true);
      } catch (e) {
        console.warn('Error closing client', e);
      }
      this.client = null;
      this.isConnected.set(false);
    }
  }

  /**
   * Set single GPIO LED output state ('1' or '0')
   */
  setSimpleLed(on: boolean): void {
    const payload = on ? '1' : '0';
    this.publish(MQTT_TOPICS.CMD_SIMPLE_SET, payload);
  }

  /**
   * Set RGB WS2812 color ("R,G,B")
   * @param immediate If true (e.g. click preset or release drag), sends immediately; otherwise throttles while dragging.
   */
  setWs2812Color(r: number, g: number, b: number, immediate = false): void {
    const payload = `${Math.round(r)},${Math.round(g)},${Math.round(b)}`;

    if (immediate) {
      if (this.throttleTimer) {
        clearTimeout(this.throttleTimer);
        this.throttleTimer = null;
      }
      this.pendingRgbPayload = null;
      this.publish(MQTT_TOPICS.CMD_WS2812_SET, payload);
      return;
    }

    // Throttle high-frequency drag events to ~60ms intervals
    this.pendingRgbPayload = payload;
    if (!this.throttleTimer) {
      this.publish(MQTT_TOPICS.CMD_WS2812_SET, payload);
      this.throttleTimer = setTimeout(() => {
        this.throttleTimer = null;
        if (this.pendingRgbPayload && this.pendingRgbPayload !== payload) {
          this.publish(MQTT_TOPICS.CMD_WS2812_SET, this.pendingRgbPayload);
        }
      }, 60);
    }
  }

  private publish(topic: string, message: string): void {
    if (!this.client || !this.isConnected()) {
      return;
    }

    this.client.publish(topic, message, { qos: 0 }, (err) => {
      if (err) {
        console.error(`Failed to publish to ${topic}:`, err);
      } else {
        this.lastActivity.set({
          topic,
          payload: message,
          time: new Date().toLocaleTimeString(),
        });
      }
    });
  }
}
