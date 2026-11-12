/**
 * StudyPet Companion App
 * Main application logic and UI controller
 */

class StudyPetApp {
  constructor() {
    this.ble = new StudyPetBLE();
    this.state = {
      timer: {
        phase: 'idle',
        remaining: 0,
        total: 0,
        cycle: 1,
        cycles: 4,
        paused: false
      },
      study: {
        active: false,
        subject: 0,
        sessionTime: 0,
        hours: [0, 0, 0, 0, 0]
      },
      deadlines: [],
      pet: {
        species: 0,
        palette: 0
      },
      settings: {
        buzzerEnabled: true,
        irEnabled: true,
        irPausesTimer: true,
        awayIdx: 2,
        repeatIdx: 2,
        brightness: 10
      }
    };

    this.subjects = ['INF1102', 'INF1103', 'INF1104', 'Revision', 'Other'];
    this.petSpecies = [
      { name: 'Cat', palettes: ['Ginger', 'Grey', 'Midnight'], emoji: '🐱' },
      { name: 'Dog', palettes: ['Golden', 'Choco', 'Spotty'], emoji: '🐶' },
      { name: 'Fox', palettes: ['Red', 'Arctic', 'Silver'], emoji: '🦊' },
      { name: 'Panda', palettes: ['Classic', 'Qinling', 'Bubblegum'], emoji: '🐼' },
      { name: 'Capybara', palettes: ['Caramel', 'Peach', 'Mocha'], emoji: '🦫' },
      { name: 'Rabbit', palettes: ['Snow', 'Cocoa', 'Grey'], emoji: '🐰' }
    ];

    this.updateInterval = null;
    this.editingDeadlineIndex = -1;
    this.deadlineSnapshot = null;
    this.dirty = { timer: false, pet: false, settings: false };
    this.timerReceivedAt = this.studyReceivedAt = 0;
    this.actionBusy = this.syncing = false;
    this.refreshPromise = null;

    this.init();
  }

  /**
   * Initialize the app
   */
  init() {
    this.setupBLE();
    this.setupEventListeners();
    this.setupServiceWorker();
    this.checkOnlineStatus();
    this.updatePetPaletteOptions();
    this.startLocalTimer();
  }

  /**
   * Setup BLE callbacks
   */
  setupBLE() {
    this.ble.onConnectionChange = (connected) => {
      this.updateConnectionStatus(connected);
      if (!connected) { clearTimeout(this.updateInterval); this.updateInterval = null; }
    };

    this.ble.onDataReceived = (data) => {
      this.handleDeviceData(data);
    };
    this.ble.onError = error => this.showToast(error.message, 'error');
  }

  /**
   * Setup all UI event listeners
   */
  setupEventListeners() {
    // Connection
    document.getElementById('connectBtn').addEventListener('click', () =>
      this.ble.connected ? this.disconnect() : this.connect());
    const action = (method, refresh = true) => () => this.runAction(() => method.call(this), refresh);

    // Tabs
    document.querySelectorAll('.tab').forEach(tab => {
      tab.addEventListener('click', () => this.switchTab(tab.dataset.tab));
    });

    // Timer controls
    document.getElementById('timerStartPause').addEventListener('click', action(this.timerToggle));
    document.getElementById('timerSkip').addEventListener('click', action(this.timerSkip));
    document.getElementById('timerReset').addEventListener('click', action(this.timerReset));
    document.getElementById('saveTimerConfig').addEventListener('click', action(this.saveTimerConfig));
    for (const id of ['timerStyle', 'focusDuration', 'shortBreak', 'longBreak', 'cycles']) {
      document.getElementById(id).addEventListener('input', () => { this.dirty.timer = true; });
    }
    for (const id of ['petSpecies', 'petPalette']) {
      document.getElementById(id).addEventListener('change', () => { this.dirty.pet = true; });
    }
    for (const id of ['buzzerEnabled', 'irEnabled', 'irPausesTimer', 'awayDelay', 'repeatAlert', 'brightness', 'tofPort']) {
      document.getElementById(id).addEventListener('input', () => { this.dirty.settings = true; });
    }

    // Timer config change handler
    document.getElementById('timerStyle').addEventListener('change', (e) => {
      const style = parseInt(e.target.value);
      if (style === 0) { // Pomodoro
        document.getElementById('focusDuration').value = 25;
        document.getElementById('shortBreak').value = 5;
        document.getElementById('longBreak').value = 15;
        document.getElementById('cycles').value = 4;
      } else if (style === 1) { // 90-min
        document.getElementById('focusDuration').value = 90;
        document.getElementById('shortBreak').value = 20;
        document.getElementById('longBreak').value = 30;
        document.getElementById('cycles').value = 2;
      }
    });

    // Study controls
    document.getElementById('studyStartStop').addEventListener('click', action(this.studyToggle));
    document.getElementById('studyDashboard').addEventListener('click', () => this.showStudyDashboard());
    document.getElementById('resetHours').addEventListener('click', action(this.resetStudyHours));

    // Deadlines
    document.getElementById('addDeadline').addEventListener('click', () => this.openDeadlineModal());
    document.getElementById('saveDeadline').addEventListener('click', action(this.saveDeadline));
    document.getElementById('deleteDeadline').addEventListener('click', action(this.deleteDeadline));
    document.getElementById('cancelDeadline').addEventListener('click', () => this.closeDeadlineModal());
    document.getElementById('closeDeadlineModal').addEventListener('click', () => this.closeDeadlineModal());

    // Pet
    document.getElementById('petSpecies').addEventListener('change', () => this.updatePetPaletteOptions());
    document.getElementById('savePet').addEventListener('click', action(this.savePet));

    // Settings
    document.getElementById('syncDateTime').addEventListener('click', action(this.syncDateTime));
    document.getElementById('saveSettings').addEventListener('click', action(this.saveSettings));
    document.getElementById('testBuzzer').addEventListener('click', action(this.testBuzzer));
    document.getElementById('refreshData').addEventListener('click', action(this.refreshAllData, false));
    document.getElementById('brightness').addEventListener('input', (e) => {
      document.getElementById('brightnessValue').textContent = e.target.value;
    });
  }

  /**
   * Setup service worker for PWA
   */
  setupServiceWorker() {
    if ('serviceWorker' in navigator) {
      navigator.serviceWorker.register('service-worker.js', { updateViaCache: 'none' })
        .then(registration => {
          console.log('Service Worker registered:', registration);
          registration.update().catch(console.warn);
        })
        .catch(error => {
          console.log('Service Worker registration failed:', error);
        });
    }
  }

  /**
   * Check online/offline status
   */
  checkOnlineStatus() {
    const updateOnlineStatus = () => {
      const notice = document.getElementById('offlineNotice');
      if (navigator.onLine) {
        notice.classList.add('hidden');
      } else {
        notice.classList.remove('hidden');
      }
    };

    window.addEventListener('online', updateOnlineStatus);
    window.addEventListener('offline', updateOnlineStatus);
    updateOnlineStatus();
  }

  /**
   * Connect to device
   */
  async connect() {
    if (this.syncing || this.ble.connecting) return;
    this.syncing = true;
    try {
      await this.ble.connect();
      document.querySelector('#connectionStatus .status-text').textContent = 'Connected · syncing…';
      await this.onDeviceConnected();
      document.querySelector('#connectionStatus .status-text').textContent = 'Connected · synced';
      this.showToast('Device data synchronized', 'success');
    } catch (error) {
      this.showToast('Connection/sync failed: ' + error.message, 'error');
    } finally {
      this.syncing = false;
      if (this.ble.connected) this.scheduleRefresh();
    }
  }

  /**
   * Update connection status UI
   */
  updateConnectionStatus(connected) {
    const status = document.getElementById('connectionStatus');
    const notice = document.getElementById('connectionNotice');
    const connectBtn = document.getElementById('connectBtn');

    if (connected) {
      status.classList.remove('disconnected');
      status.classList.add('connected');
      status.querySelector('.status-text').textContent = 'Connected';
      notice.classList.add('hidden');
      connectBtn.textContent = 'Disconnect';
    } else {
      status.classList.remove('connected');
      status.classList.add('disconnected');
      status.querySelector('.status-text').textContent = 'Disconnected';
      notice.classList.remove('hidden');
      connectBtn.textContent = 'Connect Device';
    }
  }

  /**
   * Disconnect from device
   */
  async disconnect() {
    await this.ble.disconnect();
    this.showToast('Disconnected', 'info');
  }

  /**
   * Handle device connected event
   */
  async onDeviceConnected() {
    // Deadlines persist, the software clock does not. Sync before loading them.
    await this.ble.setDateTime(Math.floor(Date.now() / 1000));
    await this.refreshAllData(true);
  }

  /**
   * Handle data received from device
   */
  handleDeviceData(data) {
    console.log('Handling device data:', data);

    switch (data.type) {
      case 'STATE':
        this.updateState(data);
        break;
      case 'TIMER':
        this.updateTimerState(data);
        break;
      case 'STUDY':
        this.updateStudyState(data);
        break;
      case 'DEADLINES':
        this.state.eepromState = data.eepromState;
        this.updateDeadlines(data.deadlines);
        this.renderDeviceStatus();
        break;
      case 'HOURS':
        this.updateStudyHours(data.hours);
        break;
      case 'PET':
        this.updatePetState(data);
        break;
      case 'SETTINGS':
        this.updateSettingsState(data);
        break;
      case 'ACK':
        break;
      case 'ERROR':
        // Command promises surface errors to the action that sent them.
        break;
    }
  }

  /**
   * Update full state
   */
  updateState(data) {
    if (data.timer) this.updateTimerState(data.timer);
    if (data.study) this.updateStudyState(data.study);
    if (data.pet) this.updatePetState(data.pet);
    if (data.settings) this.updateSettingsState(data.settings);
    if (data.range) this.state.range = data.range;
    if (data.timestamp !== undefined) {
      this.state.timestamp = data.timestamp;
      document.getElementById('deviceDateTime').value = this.epochToDatetimeLocal(data.timestamp);
    }
    if (data.eepromState !== undefined) this.state.eepromState = data.eepromState;
    document.getElementById('firmwareVersion').textContent = `BLE protocol ${data.protocol || 1}`;
    this.renderDeviceStatus();
  }

  /**
   * Update timer state and UI
   */
  updateTimerState(timer) {
    timer = { ...timer };
    if (typeof timer.phase === 'number') timer.phase = ['idle', 'focus', 'break', 'long'][timer.phase] || 'idle';
    this.state.timer = { ...this.state.timer, ...timer };
    this.timerReceivedAt = Date.now();
    if (!this.dirty.timer) {
      const fields = { timerStyle: 'style', focusDuration: 'focusMin', shortBreak: 'shortMin', longBreak: 'longMin', cycles: 'cycles' };
      for (const [id, key] of Object.entries(fields)) {
        if (timer[key] !== undefined) document.getElementById(id).value = timer[key];
      }
    }
    this.renderTimer();
  }

  /**
   * Update study state and UI
   */
  updateStudyState(study) {
    this.state.study = { ...this.state.study, ...study };
    this.studyReceivedAt = Date.now();
    this.renderStudy();
  }

  /**
   * Update pet state
   */
  updatePetState(pet) {
    this.state.pet = { ...this.state.pet, ...pet };
    this.renderPet();
  }

  /**
   * Update settings state
   */
  updateSettingsState(settings) {
    this.state.settings = { ...this.state.settings, ...settings };
    this.renderSettings();
  }

  /**
   * Update deadlines
   */
  updateDeadlines(deadlines) {
    if (this.editingDeadlineIndex >= 0 && this.deadlineSnapshot) {
      const current = deadlines.find(d => d.index === this.editingDeadlineIndex);
      if (JSON.stringify(current) !== this.deadlineSnapshot) {
        this.closeDeadlineModal();
        this.showToast('This deadline changed on the device. Open it again to edit.', 'warning');
      }
    }
    this.state.deadlines = deadlines;
    this.renderDeadlines();
  }

  /**
   * Update study hours
   */
  updateStudyHours(hours) {
    this.state.study.hours = hours;
    this.renderStudyHours();
  }

  /**
   * Render timer UI
   */
  renderTimer() {
    const { phase, cycle, cycles, paused, awayPaused, counting } = this.state.timer;
    const elapsed = this.displayElapsed(this.timerReceivedAt);
    const remaining = Math.max(0, this.state.timer.remaining - (counting ? elapsed : 0));

    // Update display
    const minutes = Math.floor(remaining / 60);
    const seconds = remaining % 60;
    document.getElementById('timerDisplay').textContent =
      `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;

    // Update phase
    let phaseText = 'Idle';
    if (phase === 'focus') phaseText = paused ? 'Paused' : 'Focus';
    else if (phase === 'break') phaseText = 'Short Break';
    else if (phase === 'long') phaseText = 'Long Break';
    if (phase !== 'idle' && awayPaused) phaseText += ' · Paused: person not detected';
    else if (phase !== 'idle' && paused) phaseText = 'Paused';
    document.getElementById('timerPhase').textContent = phaseText;

    // Update cycle info
    document.getElementById('cycleInfo').textContent =
      phase === 'idle' ? 'Not started' : `Cycle ${cycle} / ${cycles}`;

    // Update progress bar
    const config = this.state.timer;
    const total = 60 * (phase === 'focus' ? config.focusMin : phase === 'break' ? config.shortMin : phase === 'long' ? config.longMin : 0);
    const progress = total ? Math.max(0, Math.min(100, ((total - remaining) / total) * 100)) : 0;
    document.getElementById('timerProgress').style.width = `${progress}%`;

    // Update button
    const btn = document.getElementById('timerStartPause');
    if (phase === 'idle') {
      btn.textContent = 'Start';
    } else if (paused) {
      btn.textContent = 'Resume';
    } else {
      btn.textContent = 'Pause';
    }
  }

  /**
   * Render study session UI
   */
  renderStudy() {
    const { active, subject, awayPaused } = this.state.study;
    const sessionTime = this.state.study.sessionTime + (active && !awayPaused ? this.displayElapsed(this.studyReceivedAt) : 0);

    const hours = Math.floor(sessionTime / 3600);
    const minutes = Math.floor((sessionTime % 3600) / 60);
    const seconds = sessionTime % 60;

    document.getElementById('sessionTime').textContent =
      `${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;

    document.getElementById('sessionSubject').textContent =
      active ? this.subjects[subject] + (awayPaused ? ' · Paused: person not detected' : '') : 'Not studying';

    const btn = document.getElementById('studyStartStop');
    btn.textContent = active ? 'Stop Session' : 'Start Session';
    btn.className = active ? 'btn btn-danger' : 'btn btn-primary';
  }

  /**
   * Render study hours dashboard
   */
  renderStudyHours() {
    const list = document.getElementById('studyHoursList');
    const { hours, subject: activeSubject, active } = this.state.study;

    const maxHours = Math.max(...hours, 1);
    let totalSeconds = 0;

    list.innerHTML = '';
    hours.forEach((seconds, index) => {
      totalSeconds += seconds;
      const h = Math.floor(seconds / 3600);
      const m = Math.floor((seconds % 3600) / 60);
      const barWidth = (seconds / maxHours) * 100;
      const isActive = active && activeSubject === index;

      const item = document.createElement('div');
      item.className = 'hours-item' + (isActive ? ' active' : '');
      item.innerHTML = `
        <div style="flex: 1;">
          <div>${this.subjects[index]}</div>
          <div class="hours-bar" style="width: ${barWidth}%"></div>
        </div>
        <div style="font-weight: 600;">${h}h ${m}m</div>
      `;
      list.appendChild(item);
    });

    const totalH = Math.floor(totalSeconds / 3600);
    const totalM = Math.floor((totalSeconds % 3600) / 60);
    document.getElementById('totalHours').textContent = `Total: ${totalH}h ${totalM}m`;
  }

  /**
   * Render deadlines list
   */
  renderDeadlines() {
    const list = document.getElementById('deadlinesList');
    const now = Date.now() / 1000;

    if (this.state.deadlines.length === 0) {
      list.innerHTML = '<div style="text-align: center; padding: 2rem; color: var(--text-dim);">No deadlines yet</div>';
      return;
    }

    list.innerHTML = '';
    this.state.deadlines.forEach((deadline, index) => {
      const diff = deadline.due - now;
      const isOverdue = diff < 0;
      const isSoon = !isOverdue && diff < deadline.leadSeconds;

      let status = 'upcoming';
      let badge = 'Upcoming';
      if (isOverdue) {
        status = 'overdue';
        badge = 'Overdue';
      } else if (isSoon) {
        status = 'soon';
        badge = 'Soon';
      }

      const item = document.createElement('div');
      item.className = `deadline-item ${status}`;
      item.onclick = () => this.editDeadline(deadline.index);

      const dueDate = new Date(deadline.due * 1000);
      const dueDateStr = dueDate.toLocaleString();
      const relativeTime = this.formatRelativeTime(diff);

      item.innerHTML = `
        <div class="deadline-header">
          <div class="deadline-title">${this.subjects[deadline.subject]} - ${this.getKindName(deadline.kind)}</div>
          <span class="deadline-badge ${status}">${badge}</span>
        </div>
        <div class="deadline-info">
          <div>Due: ${dueDateStr}</div>
          <div class="deadline-due">${relativeTime}</div>
        </div>
      `;
      list.appendChild(item);
    });
  }

  /**
   * Render pet customization
   */
  renderPet() {
    if (this.dirty.pet) return;
    const { species, palette } = this.state.pet;
    const speciesData = this.petSpecies[species];

    document.getElementById('petSpecies').value = species;
    this.updatePetPaletteOptions();
    document.getElementById('petPalette').value = palette;

    document.querySelector('.pet-placeholder').textContent = speciesData.emoji;
    document.getElementById('petName').textContent =
      `${speciesData.name} - ${speciesData.palettes[palette]}`;
  }

  /**
   * Render settings
   */
  renderSettings() {
    if (this.dirty.settings) return;
    const s = this.state.settings;

    document.getElementById('buzzerEnabled').checked = s.buzzerEnabled;
    document.getElementById('irEnabled').checked = s.irEnabled;
    document.getElementById('irPausesTimer').checked = s.irPausesTimer;
    document.getElementById('awayDelay').value = s.awayIdx;
    document.getElementById('repeatAlert').value = s.repeatIdx;
    document.getElementById('brightness').value = s.brightness;
    document.getElementById('brightnessValue').textContent = s.brightness;
    document.getElementById('tofPort').value = s.tofPort === undefined ? 1 : s.tofPort;
  }

  renderDeviceStatus() {
    const states = ['empty', 'saved', 'backup recovered', 'corrupt', 'I/O error', 'not initialized'];
    document.getElementById('eepromStatus').textContent = states[this.state.eepromState] || 'Unknown';
    const range = this.state.range;
    document.getElementById('rangeStatus').textContent = !range ? 'Unknown' : !range.enabled ? 'Disabled' :
      `${range.valid ? range.millimeters + ' mm' : 'No valid reading'} · ${range.present ? 'Person detected' : 'Person not detected'} (limit ${range.cutoffMm} mm)`;
  }

  /**
   * Timer controls
   */
  async timerToggle() {
    if (!this.ble.connected) {
      this.showToast('Device not connected', 'warning');
      return;
    }

    const { phase, paused } = this.state.timer;
    if (phase === 'idle' || paused) {
      await this.ble.timerStart();
    } else {
      await this.ble.timerPause();
    }
  }

  async timerSkip() {
    if (!this.ble.connected) {
      this.showToast('Device not connected', 'warning');
      return;
    }
    await this.ble.timerSkip();
  }

  async timerReset() {
    if (!this.ble.connected) {
      this.showToast('Device not connected', 'warning');
      return;
    }
    await this.ble.timerReset();
  }

  async saveTimerConfig() {
    if (!this.ble.connected) {
      this.showToast('Device not connected', 'warning');
      return;
    }

    const config = {
      style: parseInt(document.getElementById('timerStyle').value),
      focusMin: parseInt(document.getElementById('focusDuration').value),
      shortMin: parseInt(document.getElementById('shortBreak').value),
      longMin: parseInt(document.getElementById('longBreak').value),
      cycles: parseInt(document.getElementById('cycles').value)
    };

    await this.ble.setTimerConfig(config);
    this.dirty.timer = false;
    this.showToast('Timer configuration saved', 'success');
  }

  /**
   * Study controls
   */
  async studyToggle() {
    if (!this.ble.connected) {
      this.showToast('Device not connected', 'warning');
      return;
    }

    if (this.state.study.active) {
      await this.ble.studyStop();
    } else {
      const subject = parseInt(document.getElementById('studySubject').value);
      await this.ble.studyStart(subject);
    }
  }

  showStudyDashboard() {
    document.getElementById('studyDashboardCard').scrollIntoView({ behavior: 'smooth' });
  }

  async resetStudyHours() {
    if (!this.ble.connected) {
      this.showToast('Device not connected', 'warning');
      return;
    }

    if (confirm('Reset all study hours? This cannot be undone.')) {
      await this.ble.resetStudyHours();
      this.showToast('Study hours reset', 'success');
    }
  }

  /**
   * Deadline management
   */
  openDeadlineModal(index = -1) {
    this.editingDeadlineIndex = index;
    const modal = document.getElementById('deadlineModal');
    const title = document.getElementById('deadlineModalTitle');
    const deleteBtn = document.getElementById('deleteDeadline');

    if (index >= 0) {
      title.textContent = 'Edit Deadline';
      deleteBtn.classList.remove('hidden');

      const deadline = this.state.deadlines.find(d => d.index === index);
      if (!deadline) { this.editingDeadlineIndex = -1; return; }
      this.deadlineSnapshot = JSON.stringify(deadline);
      document.getElementById('deadlineSubject').value = deadline.subject;
      document.getElementById('deadlineKind').value = deadline.kind;
      document.getElementById('deadlineDue').value = this.epochToDatetimeLocal(deadline.due);
      document.getElementById('deadlineRemind').value = deadline.lead;
    } else {
      this.deadlineSnapshot = null;
      title.textContent = 'New Deadline';
      deleteBtn.classList.add('hidden');

      // Default to tomorrow at 23:59
      const tomorrow = new Date();
      tomorrow.setDate(tomorrow.getDate() + 1);
      tomorrow.setHours(23, 59, 0, 0);

      document.getElementById('deadlineSubject').value = 0;
      document.getElementById('deadlineKind').value = 0;
      document.getElementById('deadlineDue').value = this.dateToDatetimeLocal(tomorrow);
      document.getElementById('deadlineRemind').value = 5; // 1 day before
    }

    modal.classList.remove('hidden');
  }

  closeDeadlineModal() {
    document.getElementById('deadlineModal').classList.add('hidden');
    this.editingDeadlineIndex = -1;
    this.deadlineSnapshot = null;
  }

  async saveDeadline() {
    if (!this.ble.connected) {
      this.showToast('Device not connected', 'warning');
      return;
    }

    const deadline = {
      subject: parseInt(document.getElementById('deadlineSubject').value),
      kind: parseInt(document.getElementById('deadlineKind').value),
      due: this.datetimeLocalToEpoch(document.getElementById('deadlineDue').value),
      lead: parseInt(document.getElementById('deadlineRemind').value)
    };
    if (!Number.isFinite(deadline.due)) throw new Error('Choose a valid deadline date and time');

    if (this.editingDeadlineIndex >= 0) {
      await this.ble.updateDeadline(this.editingDeadlineIndex, deadline);
      this.showToast('Deadline updated', 'success');
    } else {
      await this.ble.addDeadline(deadline);
      this.showToast('Deadline added', 'success');
    }

    this.closeDeadlineModal();

    // Success appears only after the firmware confirms the EEPROM write.
    await this.ble.getDeadlines();
  }

  editDeadline(index) {
    this.openDeadlineModal(index);
  }

  async deleteDeadline() {
    if (!this.ble.connected) {
      this.showToast('Device not connected', 'warning');
      return;
    }

    if (confirm('Delete this deadline?')) {
      await this.ble.deleteDeadline(this.editingDeadlineIndex);
      this.showToast('Deadline deleted', 'success');
      this.closeDeadlineModal();

      // Refresh deadlines
      await this.ble.getDeadlines();
    }
  }

  /**
   * Pet customization
   */
  updatePetPaletteOptions() {
    const species = parseInt(document.getElementById('petSpecies').value);
    const paletteSelect = document.getElementById('petPalette');
    const speciesData = this.petSpecies[species];

    paletteSelect.innerHTML = '';
    speciesData.palettes.forEach((palette, index) => {
      const option = document.createElement('option');
      option.value = index;
      option.textContent = palette;
      paletteSelect.appendChild(option);
    });

    // Update preview
    document.querySelector('.pet-placeholder').textContent = speciesData.emoji;
    document.getElementById('petName').textContent =
      `${speciesData.name} - ${speciesData.palettes[0]}`;
  }

  async savePet() {
    if (!this.ble.connected) {
      this.showToast('Device not connected', 'warning');
      return;
    }

    const species = parseInt(document.getElementById('petSpecies').value);
    const palette = parseInt(document.getElementById('petPalette').value);

    await this.ble.setPet(species, palette);
    this.dirty.pet = false;
    this.showToast('Pet customization saved', 'success');
  }

  /**
   * Settings
   */
  async syncDateTime() {
    if (!this.ble.connected) {
      this.showToast('Device not connected', 'warning');
      return;
    }

    const now = Math.floor(Date.now() / 1000);
    await this.ble.setDateTime(now);
    this.showToast('Date & time synchronized', 'success');
  }

  async saveSettings() {
    if (!this.ble.connected) {
      this.showToast('Device not connected', 'warning');
      return;
    }

    const settings = {
      buzzerEnabled: document.getElementById('buzzerEnabled').checked,
      irEnabled: document.getElementById('irEnabled').checked,
      irPausesTimer: document.getElementById('irPausesTimer').checked,
      awayIdx: parseInt(document.getElementById('awayDelay').value),
      repeatIdx: parseInt(document.getElementById('repeatAlert').value),
      brightness: parseInt(document.getElementById('brightness').value),
      tofPort: parseInt(document.getElementById('tofPort').value)
    };

    await this.ble.setSettings(settings);
    this.dirty.settings = false;
    this.showToast('Settings saved', 'success');
  }

  async testBuzzer() {
    if (!this.ble.connected) {
      this.showToast('Device not connected', 'warning');
      return;
    }

    await this.ble.testBuzzer();
  }

  async refreshAllData(silent = false) {
    if (!this.ble.connected) {
      this.showToast('Device not connected', 'warning');
      return;
    }

    if (this.refreshPromise) return this.refreshPromise;
    this.refreshPromise = (async () => {
      await this.ble.requestState();
      await this.ble.getDeadlines();
      await this.ble.getStudyHours();
      if (!silent) this.showToast('Data refreshed', 'success');
    })();
    try { await this.refreshPromise; }
    finally { this.refreshPromise = null; }
  }

  async runAction(action, refresh = true) {
    if (this.actionBusy || this.syncing) { this.showToast('Please wait for device synchronization', 'info'); return; }
    this.actionBusy = true;
    try {
      await action();
      if (refresh && this.ble.connected) {
        // Finish any older poll, then fetch a fresh snapshot AFTER the mutation.
        if (this.refreshPromise) await this.refreshPromise;
        await this.refreshAllData(true);
      }
    } catch (error) {
      this.showToast(error.message, 'error');
    } finally { this.actionBusy = false; }
  }

  scheduleRefresh() {
    clearTimeout(this.updateInterval);
    this.updateInterval = setTimeout(async () => {
      if (!this.ble.connected) return;
      try {
        if (!this.actionBusy && !this.ble.busy && !document.hidden) await this.refreshAllData(true);
      } catch (error) { this.showToast('Device sync failed: ' + error.message, 'error'); }
      finally { if (this.ble.connected) this.scheduleRefresh(); }
    }, 8000);
  }

  displayElapsed(receivedAt) {
    const elapsed = Date.now() - receivedAt;
    // Stop estimating if telemetry goes stale or the connection is lost.
    return this.ble.connected && elapsed >= 0 && elapsed <= 7000 ? Math.floor(elapsed / 1000) : 0;
  }

  /**
   * Smooth recent device telemetry without advancing paused/disconnected clocks.
   */
  startLocalTimer() {
    setInterval(() => {
      this.renderTimer();
      this.renderStudy();
    }, 1000);
  }

  /**
   * Tab switching
   */
  switchTab(tabName) {
    document.querySelectorAll('.tab').forEach(tab => {
      tab.classList.toggle('active', tab.dataset.tab === tabName);
    });

    document.querySelectorAll('.tab-content').forEach(content => {
      content.classList.toggle('active', content.id === `${tabName}Tab`);
    });
  }

  /**
   * Utility functions
   */
  showToast(message, type = 'info') {
    const toast = document.getElementById('toast');
    toast.textContent = message;
    toast.className = `toast ${type}`;
    toast.classList.remove('hidden');

    setTimeout(() => {
      toast.classList.add('hidden');
    }, 3000);
  }

  formatRelativeTime(seconds) {
    const abs = Math.abs(seconds);
    const late = seconds < 0;

    if (abs < 60) return late ? 'Just overdue' : 'Due in <1m';
    if (abs < 3600) {
      const m = Math.floor(abs / 60);
      return late ? `${m}m overdue` : `Due in ${m}m`;
    }
    if (abs < 86400) {
      const h = Math.floor(abs / 3600);
      return late ? `${h}h overdue` : `Due in ${h}h`;
    }
    const d = Math.floor(abs / 86400);
    return late ? `${d}d overdue` : `Due in ${d}d`;
  }

  getKindName(kind) {
    const kinds = ['Assignment', 'Quiz', 'Exam', 'Project', 'Lab', 'Other'];
    return kinds[kind] || 'Unknown';
  }

  dateToDatetimeLocal(date) {
    const year = date.getFullYear();
    const month = String(date.getMonth() + 1).padStart(2, '0');
    const day = String(date.getDate()).padStart(2, '0');
    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');
    return `${year}-${month}-${day}T${hours}:${minutes}`;
  }

  datetimeLocalToEpoch(datetimeLocal) {
    return Math.floor(new Date(datetimeLocal).getTime() / 1000);
  }

  epochToDatetimeLocal(epoch) {
    return this.dateToDatetimeLocal(new Date(epoch * 1000));
  }

  delay(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
  }
}

// Initialize app when DOM is ready
if (typeof module !== 'undefined') module.exports = StudyPetApp;
if (typeof document !== 'undefined' && document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', () => {
    window.app = new StudyPetApp();
  });
} else if (typeof document !== 'undefined') {
  window.app = new StudyPetApp();
}
