import autoBind from 'auto-bind';

const DEFAULT_TIMEOUT_MS = 1000;

const NAVIGATION_KEYS = new Set([
  'ArrowUp',
  'ArrowDown',
  'PageUp',
  'PageDown',
  'Home',
  'End',
  'Escape',
  'Enter',
  'Return',
  'Backspace',
]);

// Reserved for global playback shortcuts in App.js unless a type-ahead session is already active.
const GLOBAL_SHORTCUT_KEYS = new Set([' ', '-', '_', '=', '+']);

export default class TypeAheadManager {
  constructor({ onSelect, timeoutMs = DEFAULT_TIMEOUT_MS } = {}) {
    autoBind(this);
    this.onSelect = onSelect;
    this.timeoutMs = timeoutMs;
    this.buffer = '';
    this.timer = null;
  }

  reset() {
    if (this.timer) {
      clearTimeout(this.timer);
      this.timer = null;
    }
    this.buffer = '';
  }

  handleKeyDown(e, itemList) {
    if (e.metaKey || e.ctrlKey || e.altKey) return false;
    if (
      e.target.tagName === 'INPUT' ||
      e.target.tagName === 'TEXTAREA' ||
      e.target.tagName === 'SELECT'
    ) {
      return false;
    }

    if (NAVIGATION_KEYS.has(e.key)) {
      this.reset();
      return false;
    }

    if (e.key.length !== 1 || !itemList || itemList.length === 0) {
      return false;
    }

    if (this.buffer === '') {
      if (GLOBAL_SHORTCUT_KEYS.has(e.key)) return false;
      if (e.shiftKey && e.key.toLowerCase() === 'm') return false;
    }

    e.preventDefault();
    this.buffer += e.key;

    if (this.timer) clearTimeout(this.timer);
    this.timer = setTimeout(this.reset, this.timeoutMs);

    const matchIdx = this.findMatchIndex(itemList, this.buffer);
    if (matchIdx !== -1 && this.onSelect) {
      this.onSelect(matchIdx);
    }

    return true;
  }

  findMatchIndex(itemList, query) {
    const lowerQuery = query.toLowerCase();
    const startIdx = itemList[0]?.name === '..' ? 1 : 0;
    if (startIdx >= itemList.length) return -1;

    // Pass 1: exact case-insensitive prefix match
    for (let i = startIdx; i < itemList.length; i++) {
      if (itemList[i].name.toLowerCase().startsWith(lowerQuery)) {
        return i;
      }
    }

    // Pass 2: Finder-style lexicographical (>=) fallback across sorted partitions
    let bestIdx = -1;
    let bestName = null;
    for (let i = startIdx; i < itemList.length; i++) {
      const name = itemList[i].name.toLowerCase();
      if (name >= lowerQuery && (bestName === null || name < bestName)) {
        bestName = name;
        bestIdx = i;
      }
    }

    return bestIdx !== -1 ? bestIdx : itemList.length - 1;
  }
}
