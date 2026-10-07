import { Component, computed, inject, signal } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { MqttConfig, MqttService } from './services/mqtt.service';

export interface Preset {
  name: string;
  r: number;
  g: number;
  b: number;
  hex: string;
  dotColor: string;
}

@Component({
  selector: 'app-root',
  standalone: true,
  imports: [CommonModule, FormsModule],
  templateUrl: './app.html',
  styleUrl: './app.css',
})
export class App {
  protected readonly mqtt = inject(MqttService);

  // Connection states bound directly to MqttService signals
  readonly mqttConnected = this.mqtt.isConnected;
  readonly esp32Online = computed(() => this.mqtt.esp32Status() === 'online');

  // Connection settings modal state (auto-opens if no brokerUrl configured)
  readonly showSettings = signal(!this.mqtt.currentConfig().brokerUrl);
  readonly configBroker = signal(this.mqtt.currentConfig().brokerUrl);
  readonly configUser = signal(this.mqtt.currentConfig().username);
  readonly configPass = signal(this.mqtt.currentConfig().password);

  // Single GPIO LED (Pin 10) state
  readonly gpioLedOn = signal(false);

  // Addressable WS2812 LED (Pin 48) state
  readonly ws2812On = signal(false);
  readonly hue = signal(356); // 0-360
  readonly sat = signal(78); // 0-100%
  readonly val = signal(100); // 0-100%

  readonly r = signal(255);
  readonly g = signal(55);
  readonly b = signal(72);

  readonly activePreset = signal<string | null>('Off');

  // Hex color representation
  readonly hexColor = computed(() => {
    if (!this.ws2812On()) return '#000000';
    return this.rgbToHex(this.r(), this.g(), this.b());
  });

  // Presets definition matching Figma design
  readonly presets: Preset[] = [
    { name: 'Red', r: 255, g: 55, b: 72, hex: '#FF3748', dotColor: '#FF3748' },
    { name: 'Green', r: 34, g: 197, b: 94, hex: '#22C55E', dotColor: '#22C55E' },
    { name: 'Blue', r: 59, g: 130, b: 246, hex: '#3B82F6', dotColor: '#3B82F6' },
    { name: 'Amber', r: 245, g: 158, b: 11, hex: '#F59E0B', dotColor: '#F59E0B' },
    { name: 'Cyan', r: 6, g: 182, b: 212, hex: '#06B6D4', dotColor: '#06B6D4' },
    { name: 'Purple', r: 168, g: 85, b: 247, hex: '#A855F7', dotColor: '#A855F7' },
    { name: 'Warm White', r: 253, g: 230, b: 138, hex: '#FDE68A', dotColor: '#FDE68A' },
    { name: 'Off', r: 0, g: 0, b: 0, hex: '#000000', dotColor: '#1e293b' },
  ];

  private isDraggingSpectrum = false;

  // Settings Actions
  toggleSettings(): void {
    this.showSettings.update((v) => !v);
  }

  saveAndReconnect(): void {
    const newConfig: MqttConfig = {
      brokerUrl: this.configBroker(),
      username: this.configUser(),
      password: this.configPass(),
      clientId: this.mqtt.currentConfig().clientId,
    };
    this.mqtt.connect(newConfig);
    this.showSettings.set(false);
  }

  // GPIO LED Actions
  toggleGpioLed(): void {
    const newState = !this.gpioLedOn();
    this.gpioLedOn.set(newState);
    this.mqtt.setSimpleLed(newState);
  }

  setGpioLed(state: boolean): void {
    this.gpioLedOn.set(state);
    this.mqtt.setSimpleLed(state);
  }

  // WS2812 Actions
  applyPreset(preset: Preset): void {
    this.activePreset.set(preset.name);
    if (preset.name === 'Off' || (preset.r === 0 && preset.g === 0 && preset.b === 0)) {
      this.turnOffWs2812();
      return;
    }

    this.ws2812On.set(true);
    this.r.set(preset.r);
    this.g.set(preset.g);
    this.b.set(preset.b);
    this.updateHsvFromRgb(preset.r, preset.g, preset.b);
    this.mqtt.setWs2812Color(preset.r, preset.g, preset.b, true);
  }

  turnOffWs2812(): void {
    this.ws2812On.set(false);
    this.r.set(0);
    this.g.set(0);
    this.b.set(0);
    this.sat.set(0);
    this.val.set(0);
    this.activePreset.set('Off');
    this.mqtt.setWs2812Color(0, 0, 0, true);
  }

  onHueChange(newHue: number | string): void {
    this.hue.set(Number(newHue));
    if (!this.ws2812On()) {
      this.ws2812On.set(true);
      this.val.set(100);
      this.sat.set(80);
    }
    this.recalculateRgbFromHsv(false);
  }

  onRgbInputChange(channel: 'r' | 'g' | 'b', event: Event): void {
    const input = event.target as HTMLInputElement;
    let val = Math.max(0, Math.min(255, Number(input.value) || 0));

    if (channel === 'r') this.r.set(val);
    if (channel === 'g') this.g.set(val);
    if (channel === 'b') this.b.set(val);

    if (this.r() === 0 && this.g() === 0 && this.b() === 0) {
      this.ws2812On.set(false);
      this.activePreset.set('Off');
    } else {
      this.ws2812On.set(true);
      this.activePreset.set(null);
    }
    this.updateHsvFromRgb(this.r(), this.g(), this.b());
    this.mqtt.setWs2812Color(this.r(), this.g(), this.b(), true);
  }

  // Pointer drag support for 2D spectrum (works with mouse and touch)
  onSpectrumPointerDown(event: PointerEvent): void {
    const el = event.currentTarget as HTMLElement;
    el.setPointerCapture(event.pointerId);
    this.isDraggingSpectrum = true;
    this.updateFromPointer(event, el, false);
  }

  onSpectrumPointerMove(event: PointerEvent): void {
    if (!this.isDraggingSpectrum) return;
    const el = event.currentTarget as HTMLElement;
    this.updateFromPointer(event, el, false);
  }

  onSpectrumPointerUp(event: PointerEvent): void {
    if (this.isDraggingSpectrum) {
      this.isDraggingSpectrum = false;
      const el = event.currentTarget as HTMLElement;
      this.updateFromPointer(event, el, true); // send immediate payload on pointer release
    }
  }

  private updateFromPointer(event: PointerEvent, el: HTMLElement, immediate: boolean): void {
    const rect = el.getBoundingClientRect();
    const x = Math.max(0, Math.min(rect.width, event.clientX - rect.left));
    const y = Math.max(0, Math.min(rect.height, event.clientY - rect.top));

    const s = Math.round((x / rect.width) * 100);
    const v = Math.round(100 - (y / rect.height) * 100);

    this.sat.set(s);
    this.val.set(v);
    this.ws2812On.set(v > 0);
    this.recalculateRgbFromHsv(immediate);
  }

  // Native color picker support for high accessibility
  onNativeColorChange(event: Event): void {
    const input = event.target as HTMLInputElement;
    const hex = input.value;
    if (!hex || hex.length !== 7) return;

    const r = parseInt(hex.substring(1, 3), 16);
    const g = parseInt(hex.substring(3, 5), 16);
    const b = parseInt(hex.substring(5, 7), 16);

    this.r.set(r);
    this.g.set(g);
    this.b.set(b);
    this.ws2812On.set(r > 0 || g > 0 || b > 0);
    this.activePreset.set(null);
    this.updateHsvFromRgb(r, g, b);
    this.mqtt.setWs2812Color(r, g, b, true);
  }

  private recalculateRgbFromHsv(immediate = false): void {
    if (!this.ws2812On()) {
      this.r.set(0);
      this.g.set(0);
      this.b.set(0);
      this.activePreset.set('Off');
      this.mqtt.setWs2812Color(0, 0, 0, immediate);
      return;
    }

    const { r, g, b } = this.hsvToRgb(this.hue(), this.sat(), this.val());
    this.r.set(r);
    this.g.set(g);
    this.b.set(b);
    this.activePreset.set(null);
    this.mqtt.setWs2812Color(r, g, b, immediate);
  }

  private hsvToRgb(h: number, s: number, v: number): { r: number; g: number; b: number } {
    s /= 100;
    v /= 100;
    const c = v * s;
    const x = c * (1 - Math.abs(((h / 60) % 2) - 1));
    const m = v - c;

    let r1 = 0, g1 = 0, b1 = 0;
    if (h >= 0 && h < 60) { r1 = c; g1 = x; b1 = 0; }
    else if (h >= 60 && h < 120) { r1 = x; g1 = c; b1 = 0; }
    else if (h >= 120 && h < 180) { r1 = 0; g1 = c; b1 = x; }
    else if (h >= 180 && h < 240) { r1 = 0; g1 = x; b1 = c; }
    else if (h >= 240 && h < 300) { r1 = x; g1 = 0; b1 = c; }
    else { r1 = c; g1 = 0; b1 = x; }

    return {
      r: Math.round((r1 + m) * 255),
      g: Math.round((g1 + m) * 255),
      b: Math.round((b1 + m) * 255),
    };
  }

  private updateHsvFromRgb(r: number, g: number, b: number): void {
    const rNorm = r / 255;
    const gNorm = g / 255;
    const bNorm = b / 255;

    const max = Math.max(rNorm, gNorm, bNorm);
    const min = Math.min(rNorm, gNorm, bNorm);
    const delta = max - min;

    let h = 0;
    if (delta !== 0) {
      if (max === rNorm) h = ((gNorm - bNorm) / delta) % 6;
      else if (max === gNorm) h = (bNorm - rNorm) / delta + 2;
      else h = (rNorm - gNorm) / delta + 4;
      h = Math.round(h * 60);
      if (h < 0) h += 360;
    }

    const s = max === 0 ? 0 : Math.round((delta / max) * 100);
    const v = Math.round(max * 100);

    this.hue.set(h);
    this.sat.set(s);
    this.val.set(v);
  }

  private rgbToHex(r: number, g: number, b: number): string {
    const toHex = (n: number) => n.toString(16).padStart(2, '0').toUpperCase();
    return `#${toHex(r)}${toHex(g)}${toHex(b)}`;
  }
}
