const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { spawn } = require('node:child_process');
const { once } = require('node:events');
const readline = require('node:readline');
const BLE = require('../ble.js');
global.StudyPetBLE = BLE;
const App = require('../app.js');
const pause = () => new Promise(resolve => setImmediate(resolve));
const bytes = text => new TextEncoder().encode(text);
function notify(ble, text, size = 7) {
  const data = bytes(text);
  for (let i = 0; i < data.length; i += size) {
    // Nonzero offset checks that DataView slices are decoded correctly.
    const storage = new Uint8Array(size + 4);
    storage.set(data.slice(i, i + size), 2);
    ble.handleNotification(new DataView(storage.buffer, 2, Math.min(size, data.length - i)));
  }
}
function fakeTransport(responder) {
  const ble = new BLE(); ble.connected = true;
  const writes = [], commands = [];
  let buffer = '';
  const decoder = new TextDecoder();
  ble.txCharacteristic = { async writeValueWithResponse(chunk) {
    assert(chunk.length <= 20); writes.push(chunk);
    buffer += decoder.decode(chunk, { stream: true });
    if (buffer.endsWith('\n')) {
      const command = JSON.parse(buffer); commands.push(command); buffer = '';
      const response = responder(command);
      if (response) setImmediate(() => notify(ble, JSON.stringify(response) + '\n', 1));
    }
    await pause();
  }};
  return { ble, writes, commands };
}

// Minimal DOM for the production app's state mapping, forms and button handlers.
const nodes = new Map();
class Element {
  constructor() {
    this.children = []; this.events = {}; this.style = {}; this._value = '';
    this.textContent = ''; this.checked = false; this.classes = new Set();
    this.classList = { add: c => this.classes.add(c), remove: c => this.classes.delete(c),
      contains: c => this.classes.has(c), toggle: (c, on) => on ? this.classes.add(c) : this.classes.delete(c) };
  }
  set value(v) { this._value = String(v); }
  get value() { return this._value; }
  set innerHTML(v) { this._html = v; this.children = []; }
  get innerHTML() { return this._html || ''; }
  appendChild(child) { this.children.push(child); if (!this.value) this.value = child.value; }
  addEventListener(type, fn) { (this.events[type] ||= []).push(fn); }
  querySelector() { return node('statusText'); }
}
function node(id) { if (!nodes.has(id)) nodes.set(id, new Element()); return nodes.get(id); }
function setupDOM() {
  nodes.clear();
  const html = fs.readFileSync(path.join(__dirname, '../index.html'), 'utf8');
  const ids = new Set([...html.matchAll(/id="([^"]+)"/g)].map(match => match[1]));
  global.document = {
    hidden: false,
    getElementById(id) { assert(ids.has(id), `Missing HTML element ${id}`); return node(id); },
    querySelector: selector => node(selector === '.pet-placeholder' ? 'petPreview' : 'statusText'),
    querySelectorAll: () => [], createElement: () => new Element()
  };
  global.confirm = () => true;
}
class TestApp extends App {
  init() {}
  showToast(message, type) { this.lastToast = { message, type }; }
}

async function transportTests() {
  const { ble, writes, commands } = fakeTransport(c => ({ type: 'ACK', id: c.id, message: 'Saved 🦫' }));
  const received = []; ble.onDataReceived = data => received.push(data);
  const replies = await Promise.all([
    ble.sendCommand('PET_SET', { species: 4, palette: 2, label: '🦫'.repeat(12) }),
    ble.timerStart(), ble.timerPause()
  ]);
  assert.deepEqual(commands.map(c => c.cmd), ['PET_SET', 'TIMER_START', 'TIMER_PAUSE']);
  assert.deepEqual(replies.map(r => r.id), [1, 2, 3]);
  assert.equal(received[0].message, 'Saved 🦫');
  assert(writes.every(w => w.length <= 20));
  notify(ble, '{"type":"TIMER","phase":1}\n{"type":"STUDY","active":true}\n', 20);
  assert.equal(received.at(-2).type, 'TIMER'); assert.equal(received.at(-1).type, 'STUDY');
  // A device ERROR rejects the save, retains the connection, and permits the next request.
  const rejected = fakeTransport(c => ({ type: c.cmd === 'DEADLINE_ADD' ? 'ERROR' : 'ACK', id: c.id, message: 'EEPROM save unverified' }));
  await assert.rejects(rejected.ble.addDeadline({}), /EEPROM save unverified/);
  assert(rejected.ble.connected); await rejected.ble.timerReset();
  await assert.rejects(ble.sendCommand('X', { huge: 'x'.repeat(500) }), /exceeds/);
  // Missing ACK is uncertain: terminate, invalidate queued work, never replay a save.
  const silent = fakeTransport(() => null); silent.ble.commandTimeoutMs = 10;
  const first = silent.ble.addDeadline({}), second = silent.ble.timerStart();
  const outcomes = await Promise.allSettled([first, second]);
  assert(outcomes.every(outcome => outcome.status === 'rejected'));
  assert(!silent.ble.connected); assert.equal(silent.commands.length, 1);
  const stuck = new BLE(); stuck.connected = true; stuck.commandTimeoutMs = 10;
  stuck.txCharacteristic = { writeValueWithResponse: () => new Promise(() => {}) };
  await assert.rejects(stuck.timerStart(), /No device reply/);
  assert(!stuck.connected); // An ATT write that never settles must not hang the UI.
  // Partial notifications cannot leak across sessions.
  notify(ble, '{"type":"STA'); await ble.disconnect();
  assert.equal(ble.receiveBuffer, '');
  console.log('PASS: framing, UTF-8 byte chunks, serialized requests, errors, timeout and session reset');
}

async function appTests() {
  setupDOM();
  const app = new TestApp(); app.ble.connected = true;
  app.setupEventListeners();
  assert.equal(node('connectBtn').events.click.length, 1);
  app.updateConnectionStatus(true); app.updateConnectionStatus(false);
  assert.equal(node('connectBtn').onclick, undefined);
  app.updateState({ type: 'STATE', protocol: 1, timestamp: 1800000000, eepromState: 1,
    timer: { phase: 1, remaining: 1500, cycle: 1, cycles: 4, style: 0, focusMin: 25, shortMin: 5, longMin: 15, counting: false, awayPaused: true },
    study: { active: true, subject: 2, sessionTime: 600, awayPaused: true },
    pet: { species: 4, palette: 2 }, settings: { brightness: 7, tofPort: 3 },
    range: { enabled: true, valid: true, millimeters: 1200, present: false, cutoffMm: 1000 }
  });
  assert.equal(app.state.timer.phase, 'focus');
  assert.match(node('timerPhase').textContent, /person not detected/);
  assert.equal(node('timerDisplay').textContent, '25:00');
  assert.equal(node('timerProgress').style.width, '0%');
  assert.equal(node('petSpecies').value, '4'); assert.equal(node('petPalette').value, '2');
  assert.equal(node('petName').textContent, 'Capybara - Mocha');
  assert.equal(node('petPalette').children.length, 3);
  assert.equal(node('tofPort').value, '3'); assert.equal(node('eepromStatus').textContent, 'saved');
  assert.match(node('rangeStatus').textContent, /1200 mm/);
  const slots = [ { index: 6, subject: 1, kind: 2, due: 1900000000, lead: 5, leadSeconds: 86400 },
    { index: 1, subject: 0, kind: 0, due: 1910000000, lead: 0, leadSeconds: 0 } ];
  app.updateDeadlines(slots); app.renderDeadlines();
  node('deadlinesList').children[0].onclick();
  assert.equal(app.editingDeadlineIndex, 6); // Sorted position 0 is EEPROM slot 6.
  let updatedSlot;
  app.ble.updateDeadline = async index => { updatedSlot = index; };
  app.ble.getDeadlines = async () => {};
  await app.saveDeadline(); assert.equal(updatedSlot, 6);
  app.openDeadlineModal(1);
  app.updateDeadlines([{ ...slots[0] }, { ...slots[1], kind: 3 }]);
  assert.equal(app.editingDeadlineIndex, -1);
  assert.match(app.lastToast.message, /changed on the device/);
  app.dirty.settings = true; node('brightness').value = 12;
  app.updateSettingsState({ brightness: 4 }); assert.equal(node('brightness').value, '12');
  app.dirty.pet = true; node('petSpecies').value = 5;
  app.updatePetState({ species: 0, palette: 0 }); assert.equal(node('petSpecies').value, '5');
  app.timerReceivedAt = Date.now() - 3000; app.renderTimer();
  assert.equal(node('timerDisplay').textContent, '25:00'); // TOF hold never estimates a countdown.
  app.updateTimerState({ phase: 2, remaining: 240, shortMin: 5, counting: true, awayPaused: false });
  app.timerReceivedAt = Date.now() - 2000; app.renderTimer();
  assert.equal(node('timerDisplay').textContent, '03:58');
  app.ble.connected = false; app.renderTimer(); assert.equal(node('timerDisplay').textContent, '04:00');
  console.log('PASS: numeric phases, TOF holds, pet palette, settings, stable EEPROM slots and edit protection');
}

async function firmwareIntegration(executable) {
  setupDOM();
  const child = spawn(path.resolve(executable), [], { stdio: ['pipe', 'pipe', 'inherit'] });
  await once(child, 'spawn');
  const ble = new BLE(); ble.connected = true;
  const app = new TestApp(); app.ble = ble; app.setupBLE();
  const host = new Map();
  const lines = readline.createInterface({ input: child.stdout });
  lines.on('line', line => {
    const data = JSON.parse(line);
    if (data.type === 'HOST') { host.get(data.event)?.(); return; }
    notify(ble, line + '\n', 20);
  });
  ble.txCharacteristic = { async writeValueWithResponse(chunk) {
    assert(chunk.length <= 20); child.stdin.write(chunk); await pause();
  }};
  const control = (command, event) => new Promise(resolve => { host.set(event, resolve); child.stdin.write(command + '\n'); });
  try {
    await app.onDeviceConnected();
    assert.equal(app.state.timer.phase, 'idle');
    await app.runAction(() => app.timerToggle());
    assert.equal(app.state.timer.phase, 'focus'); assert.equal(app.state.timer.counting, true);
    await app.runAction(() => app.timerToggle()); assert.equal(app.state.timer.paused, true);
    await app.runAction(() => app.timerSkip()); assert.equal(app.state.timer.phase, 'break');
    await app.runAction(() => app.timerReset()); assert.equal(app.state.timer.phase, 'idle');
    node('studySubject').value = 2;
    await app.runAction(() => app.studyToggle()); assert.equal(app.state.study.subject, 2); assert(app.state.study.active);
    await app.runAction(() => app.studyToggle()); assert(!app.state.study.active);
    node('petSpecies').value = 5; node('petPalette').value = 1;
    await app.runAction(() => app.savePet()); assert.deepEqual(app.state.pet, { species: 5, palette: 1 });
    node('brightness').value = 8;
    await app.runAction(() => app.saveSettings()); assert.equal(app.state.settings.brightness, 8);
    // Sorted records occupy different physical slots. Update/delete must use the slot.
    const due = Math.floor(Date.now() / 1000) + 86400;
    const a = await ble.addDeadline({ subject: 0, kind: 0, due: due + 500, lead: 5 });
    const b = await ble.addDeadline({ subject: 1, kind: 2, due, lead: 3 });
    assert.equal(a.persistent, true); assert.equal(b.persistent, true);
    await app.refreshAllData(true); assert.equal(app.state.deadlines[0].index, b.index);
    app.openDeadlineModal(b.index); node('deadlineSubject').value = 4;
    await app.runAction(() => app.saveDeadline());
    assert.equal(app.state.deadlines.find(d => d.index === b.index).subject, 4);
    await control('@LOCAL', 'local'); await app.refreshAllData(true);
    assert.deepEqual(app.state.pet, { species: 4, palette: 2 }); assert.equal(app.state.settings.brightness, 3);
    await control('@REBOOT', 'rebooted'); await app.onDeviceConnected();
    assert.equal(app.state.deadlines.length, 2); assert.equal(app.state.eepromState, 1);
    assert.equal(app.state.deadlines.find(d => d.index === b.index).subject, 4);
    app.openDeadlineModal(b.index); await app.runAction(() => app.deleteDeadline());
    assert.deepEqual(app.state.deadlines.map(d => d.index), [a.index]);
    // Invalid configuration must surface the real firmware error, keeping state.
    await assert.rejects(ble.setTimerConfig({ style: 2, focusMin: 7, shortMin: 5, longMin: 15, cycles: 4 }), /Invalid timer/);
    console.log('PASS: PWA ↔ production BleProtocol controls, local changes, EEPROM CRUD and simulated reboot persistence');
  } finally {
    await ble.disconnect(); lines.close(); child.stdin.end(); child.kill();
  }
}

(async () => {
  await transportTests(); await appTests();
  if (process.argv[2]) await firmwareIntegration(process.argv[2]);
  else console.log('SKIP: native firmware integration (pass the compiled ble_bridge executable)');
})().catch(error => { console.error(error); process.exitCode = 1; });
