import React, { useCallback, useContext } from 'react';
import { UserContext } from './UserProvider';

export default function Announcements() {
  const { updateSettings } = useContext(UserContext);
  const setTheme = useCallback((theme) => {
    updateSettings({ theme });
  }, [updateSettings]);

  return (
    <div className="Announcements" style={{maxWidth: 544 }}>
      <p style={{font: 'var(--fontPxPlusChipPlayer)', whiteSpace: 'pre-wrap'}}>
{`╔═══════════════════════════════╗
║     W h a t ' s    N e w      ║
╚═══════════════════════════════╝
`}
      </p>
      <h3>2025-02-24</h3>
      <p>
        Added <a href="/browse/Battle%20of%20the%20Bits">Battle of the Bits</a> to the catalog. (2356 tunes so far)<br/>
        Added <a href="/browse/Nintendo%20Game%20Boy">Nintendo Game Boy</a> (.gbs) to the catalog. (1559 + 39 homebrew)<br/>
      </p>
      <h3>2025-02-16</h3>
      <p>Experimental: themes. Try out <a onClick={() => setTheme('winamp')}>Winamp theme</a> or switch back to <a onClick={() => setTheme('msdos')}>MSDOS</a>.</p>
      <h3>2025-01-25</h3>
      <p>
        Updated game-music-emu from a 2018 fork to the latest upstream (0.6.4). Among other things, this adds support for glitched SPC files (i.e. from <a href="/browse/Contemporary/SNES%20Romhacks%20(SPC)">SMWCentral romhacks</a>).
        <br/><br/>
        New for Game Music Emu player settings:<br/>
        - stereoWidth (now built into game-music-emu)<br/>
        - disableEcho (for SNES)
      </p>
      <h3>2025-01-20</h3>
      <p>
        Added song list keyboard interaction.
        Use arrow keys to navigate, enter to play, and double click to play.
        Upgraded Favorites tab to a virtualized list.
        And added Winamp-style "list view" affordances. This includes:
        <br/><br/>
        - click to select a row (the "cursor")<br/>
        - press arrow keys to move cursor up/down<br/>
        - double click to play/navigate cursor<br/>
        - press enter to play/navigate cursor<br/>
        - restore scroll position and cursor when navigating back<br/>
        - backspace to navigate back<br/>
        - press a letter to jump to the first song starting with that letter<br/>
      </p>
      <h3>2024-07-16</h3>
      <p>
        Migrate all players to load file data from the Emscripten heap (instead of stack). This fixes playback for the largest files in the catalog, e.g. <a href="?play=Contemporary/AM4N/crystal_guardian.vgm">VGM files north of 5 MB</a>.
      </p>
      <h3>2024-06-25</h3>
      <p>
        Improved voice naming for LibVGM and MDX players. Now grouped by sound chip.
      </p>
    </div>
  );
}
