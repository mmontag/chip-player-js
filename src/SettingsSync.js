// Keep unsaved settings per account until the server acknowledges the latest version.
export default class SettingsSync {
  constructor(uid, storage, send) {
    this.key = `pending-settings:${uid}`;
    this.storage = storage;
    this.send = send;
    this.pending = null;
    this.revision = 0;
    this.inFlight = false;
    this.timer = null;
    this.disposed = false;
    try {
      const saved = storage.getItem(this.key);
      if (saved) this.pending = JSON.parse(saved);
    } catch (e) {
      console.warn('Could not restore pending settings:', e);
    }
  }

  enqueue(settings) {
    this.pending = settings;
    this.revision++;
    try {
      this.storage.setItem(this.key, JSON.stringify(settings));
    } catch (e) {
      console.warn('Could not persist pending settings:', e);
    }
    this.schedule(1000);
  }

  schedule(delay) {
    if (this.disposed || !this.pending) return;
    clearTimeout(this.timer);
    this.timer = setTimeout(() => this.flush(), delay);
  }

  async flush() {
    if (this.disposed || this.inFlight || !this.pending) return;
    clearTimeout(this.timer);
    this.inFlight = true;
    const revision = this.revision;
    const settings = this.pending;
    let failed = false;
    try {
      await this.send(settings);
      if (!this.disposed && revision === this.revision) {
        this.pending = null;
        try { this.storage.removeItem(this.key); }
        catch (e) { console.warn('Could not clear pending settings:', e); }
      }
    } catch (e) {
      failed = true;
      console.warn('Could not save settings; will retry:', e);
    } finally {
      this.inFlight = false;
      if (!this.disposed && this.pending) this.schedule(failed ? 5000 : 0);
    }
  }

  dispose() {
    this.disposed = true;
    clearTimeout(this.timer);
  }
}
