/**
 * BLE Communication Layer for StudyPet
 * Handles all Bluetooth communication with the TinyCircuit device
 */

class StudyPetBLE {
  constructor() {
    // BLE Service and Characteristic UUIDs
    // These should match the UUIDs defined in your TinyCircuit firmware
    this.SERVICE_UUID = '6e400001-b5a3-f393-e0a9-e50e24dcca9e'; // Nordic UART Service
    this.TX_CHAR_UUID = '6e400002-b5a3-f393-e0a9-e50e24dcca9e'; // Write
    this.RX_CHAR_UUID = '6e400003-b5a3-f393-e0a9-e50e24dcca9e'; // Notify

    this.device = null;
    this.server = null;
    this.service = null;
    this.txCharacteristic = null;
    this.rxCharacteristic = null;
    this.connected = false;

    this.onConnectionChange = null;
    this.onDataReceived = null;

    this.decoder = new TextDecoder();
    this.encoder = new TextEncoder();
  }

  /**
   * Check if Web Bluetooth is supported
   */
  isSupported() {
    return 'bluetooth' in navigator;
  }

  /**
   * Connect to the StudyPet device
   */
  async connect() {
    if (!this.isSupported()) {
      throw new Error('Web Bluetooth is not supported in this browser');
    }

    try {
      console.log('Requesting Bluetooth device...');

      // Request device with the StudyPet service
      this.device = await navigator.bluetooth.requestDevice({
        filters: [
          { services: [this.SERVICE_UUID] }
        ],
        optionalServices: [this.SERVICE_UUID]
      });

      console.log('Device selected:', this.device.name);

      // Listen for disconnection
      this.device.addEventListener('gattserverdisconnected', () => {
        console.log('Device disconnected');
        this.connected = false;
        if (this.onConnectionChange) {
          this.onConnectionChange(false);
        }
      });

      // Connect to GATT server
      console.log('Connecting to GATT server...');
      this.server = await this.device.gatt.connect();

      // Get service
      console.log('Getting service...');
      this.service = await this.server.getPrimaryService(this.SERVICE_UUID);

      // Get characteristics
      console.log('Getting characteristics...');
      this.txCharacteristic = await this.service.getCharacteristic(this.TX_CHAR_UUID);
      this.rxCharacteristic = await this.service.getCharacteristic(this.RX_CHAR_UUID);

      // Start notifications
      await this.rxCharacteristic.startNotifications();
      this.rxCharacteristic.addEventListener('characteristicvaluechanged', (event) => {
        this.handleNotification(event.target.value);
      });

      this.connected = true;
      console.log('Connected successfully');

      if (this.onConnectionChange) {
        this.onConnectionChange(true);
      }

      return true;
    } catch (error) {
      console.error('Connection failed:', error);
      this.connected = false;
      throw error;
    }
  }

  /**
   * Disconnect from device
   */
  async disconnect() {
    if (this.device && this.device.gatt.connected) {
      await this.device.gatt.disconnect();
    }
    this.connected = false;
    if (this.onConnectionChange) {
      this.onConnectionChange(false);
    }
  }

  /**
   * Send command to device
   */
  async sendCommand(command, data = {}) {
    if (!this.connected || !this.txCharacteristic) {
      throw new Error('Device not connected');
    }

    const message = JSON.stringify({ cmd: command, data });
    const chunks = this.splitMessage(message);

    for (const chunk of chunks) {
      const bytes = this.encoder.encode(chunk);
      await this.txCharacteristic.writeValue(bytes);
      // Small delay between chunks to prevent overflow
      await this.delay(10);
    }
  }

  /**
   * Split message into chunks (BLE has MTU limits, typically 20-512 bytes)
   */
  splitMessage(message, chunkSize = 20) {
    const chunks = [];
    for (let i = 0; i < message.length; i += chunkSize) {
      chunks.push(message.substring(i, i + chunkSize));
    }
    return chunks;
  }

  /**
   * Handle incoming notification from device
   */
  handleNotification(value) {
    const text = this.decoder.decode(value);
    console.log('Received:', text);

    try {
      const data = JSON.parse(text);
      if (this.onDataReceived) {
        this.onDataReceived(data);
      }
    } catch (error) {
      console.error('Failed to parse notification:', error);
    }
  }

  /**
   * Request current state from device
   */
  async requestState() {
    await this.sendCommand('GET_STATE');
  }

  /**
   * Timer Commands
   */
  async timerStart() {
    await this.sendCommand('TIMER_START');
  }

  async timerPause() {
    await this.sendCommand('TIMER_PAUSE');
  }

  async timerSkip() {
    await this.sendCommand('TIMER_SKIP');
  }

  async timerReset() {
    await this.sendCommand('TIMER_RESET');
  }

  async setTimerConfig(config) {
    await this.sendCommand('TIMER_CONFIG', config);
  }

  /**
   * Study Session Commands
   */
  async studyStart(subject) {
    await this.sendCommand('STUDY_START', { subject });
  }

  async studyStop() {
    await this.sendCommand('STUDY_STOP');
  }

  async getStudyHours() {
    await this.sendCommand('STUDY_GET_HOURS');
  }

  async resetStudyHours() {
    await this.sendCommand('STUDY_RESET_HOURS');
  }

  /**
   * Deadline Commands
   */
  async getDeadlines() {
    await this.sendCommand('DEADLINES_GET');
  }

  async addDeadline(deadline) {
    await this.sendCommand('DEADLINE_ADD', deadline);
  }

  async updateDeadline(index, deadline) {
    await this.sendCommand('DEADLINE_UPDATE', { index, deadline });
  }

  async deleteDeadline(index) {
    await this.sendCommand('DEADLINE_DELETE', { index });
  }

  /**
   * Pet Commands
   */
  async setPet(species, palette) {
    await this.sendCommand('PET_SET', { species, palette });
  }

  /**
   * Settings Commands
   */
  async setDateTime(timestamp) {
    await this.sendCommand('SET_DATETIME', { timestamp });
  }

  async setSettings(settings) {
    await this.sendCommand('SETTINGS_SET', settings);
  }

  async testBuzzer() {
    await this.sendCommand('BUZZER_TEST');
  }

  /**
   * Utility
   */
  delay(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
  }
}

// Export for use in app.js
window.StudyPetBLE = StudyPetBLE;
