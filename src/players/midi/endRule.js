import MIDIEvents from './MIDIEvents';

const {
  EVENT_MIDI, EVENT_SYSEX, EVENT_DIVSYSEX,
  EVENT_MIDI_NOTE_ON, EVENT_MIDI_NOTE_OFF,
  EVENT_MIDI_CONTROLLER, EVENT_MIDI_PROGRAM_CHANGE, EVENT_MIDI_PITCH_BEND,
  EVENT_MIDI_CHANNEL_AFTERTOUCH, EVENT_MIDI_NOTE_AFTERTOUCH,
} = MIDIEvents;

const CC_SUSTAIN = 64;
const CC_SOSTENUTO = 66;
const CC_ALL_SOUNDS_OFF = 120;
const CC_RESET_ALL_CONTROLLERS = 121;
const CC_ALL_NOTES_OFF = 123;

/**
 * The end of a MIDI song, derived from its events in independent layers.
 *
 * A MIDI file declares no length. The only "end" it carries is its last event,
 * and that is exactly what a converter pads -- `END_OF_TRACK` a whole loop past
 * the music (Gyrocopter), or a tail of note-offs for notes that are not sounding
 * (`Voyage`). So the end has to be derived from what can be AUDIBLE, and that
 * derivation is a set of independent conditions, one per event class, so a caller
 * can enable exactly the ones it wants and measure the difference:
 *
 *   state layers      change WHAT IS SOUNDING (a state walk over the list)
 *     note            a note-on starts sound, a note-off ends it
 *     sustain         the pedal family: CC64 holds a released note, CC66 holds the
 *                     notes that were down when it landed (each hold ends when its
 *                     own switch goes off, or on CC121's reset)
 *     panic           CC120/CC123 (all sounds/notes off) ends it immediately
 *
 *   trailing layers   cannot hold a note at all; enabling one only extends the
 *                     end to its last event. They exist to measure what
 *                     over-including costs -- and `meta` alone reproduces the
 *                     "last event wins" behaviour the unpatched merge had.
 *     controller, program, bend, aftertouch, sysex, meta
 *
 * Which control messages can change what is sounding, and where each one stands here
 * (MIDI 1.0 / RP-015), so the next reader does not rediscover it one round at a time:
 *
 *   modelled      64 damper                 holds a released note until it lifts
 *                 66 sostenuto              holds the notes that were DOWN when it
 *                                          landed -- a snapshot, not a running hold
 *                 120 all sounds off        silences the channel, holds included,
 *                                          without changing the pedals' own values
 *                 121 reset all controllers releases the pedal family, nothing else
 *                 123 all notes off         silences what is not pedal-held
 *
 *   not a hold     5 portamento time, 65 portamento, 67 soft, 68 legato, 84 portamento
 *                  control: they change how notes play, not whether they sound.
 *                  69 hold 2 lengthens the RELEASE rather than holding the note
 *                  ("the note may eventually fade out despite the musician still
 *                  holding down the key and this pedal"), so it stays out.
 *
 *   out of reach   124-127 omni/mono/poly mode and sysex resets (GM/GS/XG) can reset a
 *                  channel, which would leave this walk stale exactly the way CC121
 *                  did. No catalog file uses them, they are a live-performance idiom,
 *                  and the `sysex` layer is the knob for the sysex half. A program
 *                  change resetting controllers is receiver behaviour, not a message
 *                  this walk can see.
 *
 * The default is the standard we trust: notes, the pedal, and all-sounds-off.
 * Nothing here knows a file path, an engine, or a setting -- see `endLayersFor`
 * for the policy, which is the one place that decides who gets which layers.
 */
export const END_LAYERS = {
  note: true,
  sustain: true,
  panic: true,
  controller: false,
  program: false,
  bend: false,
  aftertouch: false,
  sysex: false,
  meta: false,
};

/**
 * The policy: which layers apply here. Returning `null` means "leave the event
 * list exactly as the merge built it" (master's behaviour).
 *
 * This is deliberately the only place a caller's conditions are read, so the
 * settings can key off whatever the caller has. `input.useTrackLoops` is the
 * SoundFont-MIDI gate `MIDIFilePlayer` already passes; a path, a user setting or
 * another data-flow flag would go here the same way. Today: the standard layers,
 * everywhere, to measure the effect on every file before narrowing it.
 */
export function endLayersFor(input = {}) {
  void input.useTrackLoops;
  return { ...END_LAYERS };
}

/**
 * The last moment anything can be audible in `events`, in ms. 0 when the enabled
 * layers never find one (the caller then leaves the list alone).
 */
export function songEndMs(events, layers = END_LAYERS) {
  let endMs = 0;
  if (!layers || (!layers.note && !layers.sustain && !layers.panic &&
      !layers.controller && !layers.program && !layers.bend &&
      !layers.aftertouch && !layers.sysex && !layers.meta)) {
    return 0;
  }

  // Sounding state, for the state layers. `pedalHeld` is a note whose note-off
  // already ran while the damper was down; `sostoHeld` is a note the sostenuto
  // pedal took when it landed -- the spec's "notes that were On", a snapshot, so a
  // note struck afterwards is not in it. Either sounds until its own pedal comes up,
  // and each hands the note to the other when that one is still down.
  const sounding = new Set();
  const pedalHeld = new Set();
  const sostoHeld = new Set();
  const pedal = new Map();
  const sostenuto = new Map();
  const held = () => layers.note && (sounding.size + pedalHeld.size + sostoHeld.size) > 0;
  const channelOf = (key) => Number(key.split('-')[0]);

  for (const e of events) {
    if (e.type !== EVENT_MIDI) {
      if (layers.sysex && (e.type === EVENT_SYSEX || e.type === EVENT_DIVSYSEX)) {
        endMs = Math.max(endMs, e.playTime);
      } else if (layers.meta && e.type !== EVENT_SYSEX && e.type !== EVENT_DIVSYSEX) {
        endMs = Math.max(endMs, e.playTime);
      }
      // A note still sounding when a trailing event arrives is audible *at* that
      // event, so a file that never releases its final note rings to its track
      // end rather than to the note's own start -- crediting only the start would
      // silence it. The layers above stay the separate question of where a file
      // ends when nothing is sounding.
      if (held()) endMs = Math.max(endMs, e.playTime);
      continue;
    }

    const key = `${e.channel}-${e.param1}`;
    // An event is part of the audible stretch if something was sounding when it
    // arrived OR is sounding after it. The first half is what credits a release:
    // a note-off (or the pedal lift that ends a held chord) IS the end.
    const wasHeld = held();
    switch (e.subtype) {
      case EVENT_MIDI_NOTE_ON:
        if (e.param2 > 0) {
          sounding.add(key);
          pedalHeld.delete(key);
          // A restruck note is a new note: sostenuto holds the notes that were down
          // when it landed, not every later note on that pitch.
          sostoHeld.delete(key);
        } else {
          endNote(e, key);
        }
        break;
      case EVENT_MIDI_NOTE_OFF:
        endNote(e, key);
        break;
      case EVENT_MIDI_CONTROLLER:
        if (e.param1 === CC_SUSTAIN && layers.sustain) {
          if (e.param2 < 64) clearHeld(e.channel);
          pedal.set(e.channel, e.param2);
        } else if (e.param1 === CC_SOSTENUTO && layers.sustain) {
          // Down: take the notes that are sounding on that channel right now.
          // Up: release them, handing each to the damper if it is still down.
          // Only the up-to-down transition captures. A repeated down while the pedal is
          // already down is a no-op in the spec, and re-snapshotting there would take
          // notes that started *after* the pedal went down -- which the note-on above
          // deliberately clears from the hold -- and hold them past their own note-off.
          if (e.param2 >= 64) {
            if ((sostenuto.get(e.channel) || 0) < 64) {
              for (const k of sounding) if (channelOf(k) === e.channel) sostoHeld.add(k);
            }
          } else {
            clearSosto(e.channel);
          }
          sostenuto.set(e.channel, e.param2);
        } else if (e.param1 === CC_RESET_ALL_CONTROLLERS && layers.panic) {
          // RP-015 turns the whole pedal family off, so the holds it was carrying are
          // released -- but it is not an all-notes-off: a note still held down keeps
          // sounding. The values are zeroed because a reset changes them; the holds are
          // dropped rather than handed over, so nothing here depends on statement order.
          pedal.set(e.channel, 0);
          sostenuto.set(e.channel, 0);
          clearHeld(e.channel);
          dropSosto(e.channel);
        } else if ((e.param1 === CC_ALL_SOUNDS_OFF || e.param1 === CC_ALL_NOTES_OFF) && layers.panic) {
          // The two panic messages are not the same thing. All Sounds Off silences
          // the channel outright, pedal-held notes included; All Notes Off leaves
          // whatever the pedals are holding, which is what the spec says it does --
          // so those notes move to the damper set, exactly where a note-off would
          // have put them. Neither changes a pedal's OWN state (only CC64/CC66 do),
          // and resetting it here would make a later note-off under a still-down
          // pedal not count as held, cutting that ending instead.
          const pedalDown = (pedal.get(e.channel) || 0) >= 64;
          for (const k of [...sounding]) {
            if (channelOf(k) !== e.channel) continue;
            sounding.delete(k);
            if (e.param1 === CC_ALL_NOTES_OFF && layers.sustain && pedalDown) pedalHeld.add(k);
          }
          if (e.param1 === CC_ALL_SOUNDS_OFF) {
            // Silenced outright, so both holds are *dropped*: handing the sostenuto's
            // notes to the damper (what clearSosto does) would let a note this message
            // already silenced keep the song open. The pedal values stay as they are --
            // All Sounds Off changes no pedal for the notes that follow.
            clearHeld(e.channel);
            dropSosto(e.channel);
          }
        } else if (layers.controller) {
          endMs = Math.max(endMs, e.playTime);
        }
        break;
      case EVENT_MIDI_PROGRAM_CHANGE:
        if (layers.program) endMs = Math.max(endMs, e.playTime);
        break;
      case EVENT_MIDI_PITCH_BEND:
        if (layers.bend) endMs = Math.max(endMs, e.playTime);
        break;
      case EVENT_MIDI_CHANNEL_AFTERTOUCH:
      case EVENT_MIDI_NOTE_AFTERTOUCH:
        if (layers.aftertouch) endMs = Math.max(endMs, e.playTime);
        break;
      default:
    }

    if (wasHeld || held()) endMs = Math.max(endMs, e.playTime);
  }

  // A note-off that finds the pedal down hands the note to `pedalHeld`: it stops
  // being held down but keeps sounding, and only the pedal lift (or a panic)
  // ends it. Leaving it in `sounding` too would keep it "sounding" after the
  // lift, so the note must move, not be copied.
  function endNote(e, key) {
    // Only a note that was playing is handed to the pedal. A converter's trailing
    // note-off for a note that never sounded would otherwise sit in `pedalHeld`,
    // keep `held()` true, and stretch the end to the pedal release (or the track
    // end) with nothing playing. The same test makes a second note-off for a note
    // the pedal already holds a no-op, which is what it is: the hold was taken
    // when the note stopped sounding.
    const wasSounding = sounding.delete(key);
    if (wasSounding && layers.sustain && (pedal.get(e.channel) || 0) >= 64) pedalHeld.add(key);
  }
  function clearHeld(channel) {
    for (const k of [...pedalHeld]) if (channelOf(k) === channel) pedalHeld.delete(k);
  }
  // Sostenuto's OWN release hands each note it was holding to the damper if that is
  // still down -- the same move `endNote` makes, in the other direction. A panic that
  // silences the channel instead drops them (`dropSosto`): transferring there would
  // put a note the message already silenced back into `pedalHeld`.
  function dropSosto(channel) {
    for (const k of [...sostoHeld]) if (channelOf(k) === channel) sostoHeld.delete(k);
  }
  function clearSosto(channel) {
    for (const k of [...sostoHeld]) {
      if (channelOf(k) !== channel) continue;
      sostoHeld.delete(k);
      if (layers.sustain && (pedal.get(channel) || 0) >= 64) pedalHeld.add(k);
    }
  }

  return endMs;
}
