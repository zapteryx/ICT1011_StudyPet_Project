/* ASD2116 UART transport. See ../docs/BLE.md for the firmware wire contract. */
class StudyPetBLE {
  constructor() {
    this.SERVICE_UUID = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
    this.TX_CHAR_UUID = '6e400002-b5a3-f393-e0a9-e50e24dcca9e';
    this.RX_CHAR_UUID = '6e400003-b5a3-f393-e0a9-e50e24dcca9e';
    this.connected = false;
    this.connecting = false;
    this.device = this.server = this.service = null;
    this.txCharacteristic = this.rxCharacteristic = null;
    this.onConnectionChange = this.onDataReceived = this.onError = null;
    this.encoder = new TextEncoder();
    this.decoder = new TextDecoder();
    this.receiveBuffer = '';
    this.requestId = 0;
    this.session = 0;
    this.commandQueue = Promise.resolve();
    this.pending = null;
    this.queued = 0;
    this.commandTimeoutMs = 12000;
    this.notificationListener = event => this.handleNotification(event.target.value);
    this.disconnectListener = () => this.clearSession(new Error('Device disconnected'));
  }

  get busy() { return this.queued > 0; }
  isSupported() { return 'bluetooth' in navigator; }

  async connect() {
    if (this.connected) return true;
    if (this.connecting) throw new Error('Connection already in progress');
    if (!this.isSupported()) throw new Error('Web Bluetooth is not supported in this browser');
    this.connecting = true;
    try {
      this.device = await navigator.bluetooth.requestDevice({
        filters: [{ services: [this.SERVICE_UUID] }],
        optionalServices: [this.SERVICE_UUID]
      });
      this.device.addEventListener('gattserverdisconnected', this.disconnectListener);
      this.server = await this.device.gatt.connect();
      this.service = await this.server.getPrimaryService(this.SERVICE_UUID);
      this.txCharacteristic = await this.service.getCharacteristic(this.TX_CHAR_UUID);
      this.rxCharacteristic = await this.service.getCharacteristic(this.RX_CHAR_UUID);
      // Attach BEFORE subscribing: the device immediately streams a full STATE.
      this.rxCharacteristic.addEventListener('characteristicvaluechanged', this.notificationListener);
      await this.rxCharacteristic.startNotifications();
      if (!this.server.connected) throw new Error('Device disconnected during setup');
      this.connected = true;
      if (this.onConnectionChange) this.onConnectionChange(true);
      return true;
    } catch (error) {
      await this.disconnect();
      throw error;
    } finally {
      this.connecting = false;
    }
  }

  clearSession(reason) {
    const wasConnected = this.connected;
    this.connected = false;
    ++this.session; // Invalidates old queued work, even across reconnects.
    if (this.pending) {
      clearTimeout(this.pending.timeout);
      this.pending.reject(reason);
      this.pending = null;
    }
    if (this.rxCharacteristic) {
      this.rxCharacteristic.removeEventListener('characteristicvaluechanged', this.notificationListener);
    }
    if (this.device) this.device.removeEventListener('gattserverdisconnected', this.disconnectListener);
    this.txCharacteristic = this.rxCharacteristic = this.service = this.server = null;
    this.decoder = new TextDecoder();
    this.receiveBuffer = '';
    if (wasConnected && this.onConnectionChange) this.onConnectionChange(false);
  }

  async disconnect() {
    const device = this.device;
    this.clearSession(new Error('Device disconnected'));
    if (device && device.gatt.connected) device.gatt.disconnect();
    this.device = null;
  }

  sendCommand(command, data = {}) {
    const session = this.session;
    ++this.queued;
    const result = this.commandQueue.then(() => {
      if (session !== this.session || !this.connected) throw new Error('Device not connected');
      return this.performCommand(command, data);
    });
    // One complete command/reply at a time, including simultaneous button taps.
    this.commandQueue = result.catch(() => {});
    return result.finally(() => { --this.queued; });
  }

  async performCommand(command, data) {
    const id = ++this.requestId;
    const body = this.encoder.encode(JSON.stringify({ cmd: command, data, id }));
    if (body.length > 384) throw new Error('Command exceeds the device buffer');
    const bytes = new Uint8Array(body.length + 1);
    bytes.set(body); bytes[body.length] = 10;
    const replyType = ({ GET_STATE: 'STATE', DEADLINES_GET: 'DEADLINES', STUDY_GET_HOURS: 'HOURS' })[command] || 'ACK';
    const result = new Promise((resolve, reject) => {
      const timeout = setTimeout(() => reject(new Error(
        'No device reply. Reconnect and refresh before retrying a save; it may already have completed.'
      )), this.commandTimeoutMs);
      this.pending = { id, replyType, resolve, reject, timeout };
    });
    // Handle rejection immediately, even if a timeout occurs during a slow write.
    const outcome = result.then(value => ({ value }), error => ({ error }));
    const interrupted = outcome.then(reply => {
      if (reply.error) throw reply.error;
      return new Promise(() => {}); // A valid reply does not interrupt a write.
    });
    try {
      for (let offset = 0; offset < bytes.length; offset += 20) {
        if (!this.connected) throw new Error('Device disconnected');
        const chunk = bytes.slice(offset, offset + 20);
        if (this.txCharacteristic.writeValueWithResponse) {
          await Promise.race([this.txCharacteristic.writeValueWithResponse(chunk), interrupted]);
        } else {
          await Promise.race([this.txCharacteristic.writeValue(chunk), interrupted]);
        }
      }
      const reply = await outcome;
      if (reply.error) throw reply.error;
      if (reply.value.type === 'ERROR') throw new Error(reply.value.message || 'Device rejected command');
      return reply.value;
    } catch (error) {
      // Reset partial commands on transport failure; never replay mutations.
      if (this.pending && this.pending.id === id) await this.disconnect();
      throw error;
    } finally {
      if (this.pending && this.pending.id === id) {
        clearTimeout(this.pending.timeout); this.pending = null;
      }
    }
  }

  handleNotification(value) {
    this.receiveBuffer += this.decoder.decode(value, { stream: true });
    let end;
    while ((end = this.receiveBuffer.indexOf('\n')) >= 0) {
      const line = this.receiveBuffer.slice(0, end).trim();
      this.receiveBuffer = this.receiveBuffer.slice(end + 1);
      if (!line) continue;
      let data;
      try { data = JSON.parse(line); }
      catch (_) { this.protocolError('Invalid JSON received from StudyPet'); return; }
      if (!data || typeof data !== 'object' || typeof data.type !== 'string') {
        this.protocolError('Invalid StudyPet response'); return;
      }
      const pending = this.pending;
      if (pending && ((data.id === pending.id && (data.type === pending.replyType || data.type === 'ERROR')) ||
                      (data.id === undefined && data.type === 'ERROR'))) {
        clearTimeout(pending.timeout); this.pending = null;
        pending.resolve(data);
      }
      if (this.onDataReceived) this.onDataReceived(data);
    }
    if (this.receiveBuffer.length > 8192) this.protocolError('StudyPet response buffer overflow');
  }

  protocolError(message) {
    this.disconnect();
    if (this.onError) this.onError(new Error(message));
  }

  requestState() { return this.sendCommand('GET_STATE'); }
  timerStart() { return this.sendCommand('TIMER_START'); }
  timerPause() { return this.sendCommand('TIMER_PAUSE'); }
  timerSkip() { return this.sendCommand('TIMER_SKIP'); }
  timerReset() { return this.sendCommand('TIMER_RESET'); }
  setTimerConfig(config) { return this.sendCommand('TIMER_CONFIG', config); }
  studyStart(subject) { return this.sendCommand('STUDY_START', { subject }); }
  studyStop() { return this.sendCommand('STUDY_STOP'); }
  getStudyHours() { return this.sendCommand('STUDY_GET_HOURS'); }
  resetStudyHours() { return this.sendCommand('STUDY_RESET_HOURS'); }
  getDeadlines() { return this.sendCommand('DEADLINES_GET'); }
  addDeadline(deadline) { return this.sendCommand('DEADLINE_ADD', deadline); }
  updateDeadline(index, deadline) { return this.sendCommand('DEADLINE_UPDATE', { index, deadline }); }
  deleteDeadline(index) { return this.sendCommand('DEADLINE_DELETE', { index }); }
  setPet(species, palette) { return this.sendCommand('PET_SET', { species, palette }); }
  // Stored local deadlines use Singapore time, independent of phone timezone.
  setDateTime(timestamp) { return this.sendCommand('SET_DATETIME', { timestamp, utcOffsetMinutes: 480 }); }
  setSettings(settings) { return this.sendCommand('SETTINGS_SET', settings); }
  testBuzzer() { return this.sendCommand('BUZZER_TEST'); }
}
if (typeof window !== 'undefined') window.StudyPetBLE = StudyPetBLE;
if (typeof module !== 'undefined') module.exports = StudyPetBLE;
