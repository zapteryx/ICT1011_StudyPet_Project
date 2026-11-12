// Real browser/DOM smoke test with a simulated GATT peripheral, not real hardware.
const assert = require('node:assert/strict');
const http = require('node:http');
const fs = require('node:fs/promises');
const path = require('node:path');
const { chromium } = require(process.env.STUDYPET_PLAYWRIGHT || 'playwright');
const root = path.resolve(__dirname, '..');
const server = http.createServer(async (request, response) => {
  try {
    const pathname = new URL(request.url, 'http://localhost').pathname.replace(/^\/course\/studypet(?=\/)/, '');
    const file = path.resolve(root, '.' + pathname + (pathname.endsWith('/') ? 'index.html' : ''));
    if (!file.startsWith(root + path.sep)) { response.writeHead(403).end(); return; }
    const content = await fs.readFile(file);
    const types = { '.js': 'application/javascript', '.html': 'text/html', '.css': 'text/css', '.svg': 'image/svg+xml', '.json': 'application/json' };
    response.writeHead(200, { 'Content-Type': types[path.extname(file)] || 'text/plain' }); response.end(content);
  } catch (_) { response.writeHead(404).end(); }
});

(async () => {
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  try {
    const page = await browser.newPage({ viewport: { width: 390, height: 844 } });
    const errors = []; page.on('pageerror', error => errors.push(error.message));
    await page.addInitScript(() => {
      const radio = window.radio = { commands: [], chunks: [], failNextSave: false,
        state: { type: 'STATE', protocol: 1, timestamp: 1800000000, eepromState: 1,
          timer: { phase: 0, remaining: 0, cycle: 1, cycles: 4, paused: false, awayPaused: false, counting: false, style: 0, focusMin: 25, shortMin: 5, longMin: 15 },
          study: { active: false, subject: 0, sessionTime: 0, awayPaused: false },
          pet: { species: 4, palette: 2 },
          settings: { buzzerEnabled: true, irEnabled: true, irPausesTimer: true, brightness: 7, awayIdx: 2, repeatIdx: 2, tofPort: 1 },
          range: { enabled: true, valid: true, millimeters: 1200, cutoffMm: 1000, present: false } },
        deadlines: [ { index: 6, subject: 1, kind: 2, due: 1900000000, lead: 5, leadSeconds: 86400 },
          { index: 1, subject: 0, kind: 0, due: 1910000000, lead: 0, leadSeconds: 0 } ]
      };
      const rx = new EventTarget();
      const notify = data => {
        const bytes = new TextEncoder().encode(JSON.stringify(data) + '\n');
        for (let offset = 0; offset < bytes.length; offset += 20) {
          const chunk = bytes.slice(offset, offset + 20);
          rx.value = new DataView(chunk.buffer); rx.dispatchEvent(new Event('characteristicvaluechanged'));
        }
      };
      rx.startNotifications = async () => { notify(radio.state); return rx; };
      let buffer = '';
      const tx = { async writeValueWithResponse(chunk) {
        if (chunk.length > 20) throw new Error('Oversized ATT write');
        radio.chunks.push([...chunk]); buffer += new TextDecoder().decode(chunk);
        if (!buffer.endsWith('\n')) return;
        const command = JSON.parse(buffer); buffer = ''; radio.commands.push(command);
        const { cmd, data, id } = command;
        let reply = { type: 'ACK', id, message: 'Applied' };
        if (radio.failNextSave && cmd.startsWith('DEADLINE_')) {
          radio.failNextSave = false; reply = { type: 'ERROR', id, message: 'EEPROM save unverified' };
        } else if (cmd === 'GET_STATE') reply = { ...radio.state, id };
        else if (cmd === 'DEADLINES_GET') reply = { type: 'DEADLINES', id, eepromState: 1, deadlines: radio.deadlines };
        else if (cmd === 'STUDY_GET_HOURS') reply = { type: 'HOURS', id, hours: [3600, 7200, 0, 0, 0] };
        else if (cmd === 'PET_SET') radio.state.pet = { ...data };
        else if (cmd === 'SETTINGS_SET') Object.assign(radio.state.settings, data);
        else if (cmd === 'TIMER_START') Object.assign(radio.state.timer, { phase: 1, remaining: 1500, paused: false, awayPaused: true, counting: false });
        else if (cmd === 'TIMER_PAUSE') radio.state.timer.paused = true;
        else if (cmd === 'DEADLINE_UPDATE') {
          Object.assign(radio.deadlines.find(d => d.index === data.index), data.deadline);
          reply.persistent = true; reply.index = data.index;
        }
        setTimeout(() => notify(reply), 0);
      }};
      const device = new EventTarget();
      device.gatt = { connected: false, async connect() { this.connected = true; return this; },
        async getPrimaryService() { return { async getCharacteristic(uuid) { return uuid.startsWith('6e400002') ? tx : rx; } }; },
        disconnect() { this.connected = false; device.dispatchEvent(new Event('gattserverdisconnected')); }
      };
      Object.defineProperty(navigator, 'bluetooth', { value: { async requestDevice() { return device; } } });
    });
    await page.goto(`http://127.0.0.1:${server.address().port}/`);
    await page.evaluate(() => navigator.serviceWorker.ready);
    assert(await page.evaluate(() => caches.has('studypet-v1.1.0')));
    await page.click('#connectBtn');
    await page.waitForFunction(() => document.querySelector('.status-text').textContent.includes('synced'));
    assert.equal(await page.inputValue('#petSpecies'), '4'); assert.equal(await page.inputValue('#petPalette'), '2');
    assert.match(await page.textContent('#totalHours'), /3h 0m/);
    await page.click('#timerStartPause');
    await page.waitForFunction(() => !app.actionBusy && app.state.timer.phase === 'focus');
    assert.match(await page.textContent('#timerPhase'), /person not detected/);
    await page.click('[data-tab="pet"]'); await page.selectOption('#petSpecies', '5'); await page.selectOption('#petPalette', '1');
    await page.click('#savePet'); await page.waitForFunction(() => !app.actionBusy && app.state.pet.species === 5);
    assert.equal(await page.textContent('#petName'), 'Rabbit - Cocoa');
    await page.click('[data-tab="deadlines"]'); await page.click('.deadline-item >> nth=0');
    await page.selectOption('#deadlineSubject', '4'); await page.click('#saveDeadline');
    await page.waitForFunction(() => !app.actionBusy && radio.deadlines[0].subject === 4);
    assert.equal(await page.evaluate(() => radio.commands.find(c => c.cmd === 'DEADLINE_UPDATE').data.index), 6);
    await page.click('.deadline-item >> nth=0'); await page.evaluate(() => { radio.failNextSave = true; });
    await page.click('#saveDeadline'); await page.waitForFunction(() => !app.actionBusy);
    assert.match(await page.textContent('#toast'), /EEPROM save unverified/);
    assert(await page.isVisible('#deadlineModal')); await page.click('#cancelDeadline');
    await page.evaluate(() => { radio.state.pet = { species: 2, palette: 1 }; radio.state.settings.brightness = 4; });
    // Local button changes must arrive through the automatic full-state poll.
    await page.waitForFunction(() => app.state.pet.species === 2, null, { timeout: 15000 });
    await page.click('[data-tab="settings"]'); await page.click('#refreshData');
    await page.waitForFunction(() => !app.actionBusy && app.state.pet.species === 2);
    assert.equal(await page.inputValue('#brightness'), '4');
    assert.equal(await page.textContent('#eepromStatus'), 'saved');
    assert.match(await page.textContent('#rangeStatus'), /1200 mm/);
    // A refresh updates the model but preserves a user's unsaved form values.
    await page.locator('#brightness').fill('9');
    await page.evaluate(() => { radio.state.settings.brightness = 5; });
    await page.click('#refreshData'); await page.waitForFunction(() => !app.actionBusy && app.state.settings.brightness === 5);
    assert.equal(await page.inputValue('#brightness'), '9');
    await page.click('#saveSettings'); await page.waitForFunction(() => !app.actionBusy && app.state.settings.brightness === 9);
    await page.screenshot({ path: path.resolve('.verification/pwa-mobile.png'), fullPage: true });
    await page.click('#connectBtn'); await page.waitForFunction(() => !app.ble.connected);
    assert.equal(await page.evaluate(() => radio.commands.filter(c => c.cmd === 'SET_DATETIME').length), 1);
    assert.deepEqual(errors, []);
    // A repository/subfolder host must keep scripts and offline assets scoped.
    await page.goto(`http://127.0.0.1:${server.address().port}/course/studypet/`);
    await page.evaluate(() => navigator.serviceWorker.ready);
    assert(await page.evaluate(async () => {
      const cache = await caches.open('studypet-v1.1.0');
      return !!(await cache.match(new URL('ble.js?v=1.1', location.href)));
    }));
    await page.context().setOffline(true);
    await page.reload();
    await page.waitForFunction(() => !!window.app);
    assert.equal(await page.textContent('#connectBtn'), 'Connect Device');
    await page.context().setOffline(false);
    console.log('PASS: mobile browser sync, controls, EEPROM slot editing/errors, local changes and disconnect');
    console.log('PASS: service worker install, subfolder hosting and offline reload');
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; }).finally(() => server.close());
