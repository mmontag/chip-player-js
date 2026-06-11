# SoundFonts

SoundFonts here are for dev purposes.
Only a few SoundFonts are tracked to keep the size of the repository down.

SoundFonts, like the music catalog, should be deployed separately from the React client build, defined in src/config/index.js.
For example, if you deploy the catalog to example.com/music and soundfonts to example.com/soundfonts:

```
 CATALOG_PREFIX = 'https://example.com/music';
 SOUNDFONT_URL_PATH = 'https://example.com/soundfonts';
```
