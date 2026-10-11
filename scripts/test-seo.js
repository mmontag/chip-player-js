const assert = require('assert');
const {
  resolveDirectoryMetadata,
  PREFIX_STRUCTURE_MAP,
  setStrategyMode,
  getStrategyMode,
  formatSimplifiedPathTitle,
} = require('../server/directory-metadata');

console.log('Running SEO Structural Directory Metadata & Path Strategy Test Suite...\n');

// 1. STRUCTURAL STRATEGY & FORMAT DETECTION TESTS
setStrategyMode('structural');

const structuralTestCases = [
  {
    path: 'Famicompo/mini5/Cover',
    expectedStrategy: 'compo_event',
    expectedTitle: 'Famicompo mini5 Cover (NSF)',
    expectedFormat: 'NSF',
  },
  {
    path: 'Famicompo (NSFE)/mini5/Cover',
    expectedStrategy: 'compo_event',
    expectedTitle: 'Famicompo mini5 Cover (NSFE)',
    expectedFormat: 'NSFE',
  },
  {
    path: 'Contemporary/2A03 Puritans',
    expectedStrategy: 'artist_discography',
    expectedTitle: '2A03 Puritans · Contemporary (NSFE)',
    expectedFormat: 'NSFE',
  },
  {
    path: 'Contemporary/Brad Smith/MOON8 (Pink Floyd)',
    expectedStrategy: 'artist_discography',
    expectedTitle: 'MOON8 (Pink Floyd) · Brad Smith (NSFE)',
    expectedFormat: 'NSFE',
  },
  {
    path: 'Contemporary/James Paddock/Doom Projects/Complete/Jenesis',
    expectedStrategy: 'artist_discography',
    expectedTitle: 'Jenesis · James Paddock (Doom Projects) (MIDI)',
    expectedFormat: 'MIDI',
  },
  {
    path: 'High Voltage SID Collection/MUSICIANS/T/Tel_Jeroen',
    expectedStrategy: 'partitioned_artist',
    expectedTitle: 'Jeroen Tel (Tel_Jeroen) · HVSC (SID)',
    expectedFormat: 'SID',
  },
  {
    path: 'The Module Collection/ARTIST-S/SP',
    expectedStrategy: 'partitioned_artist',
    expectedTitle: 'SP · The Module Collection (MOD)',
    expectedFormat: 'MOD',
  },
  {
    path: 'VGM Rips/Arcade/Capcom/1941 - Counter Attack (CP System)',
    expectedStrategy: 'platform_game_archive',
    expectedFormat: 'VGM',
  },
  {
    path: 'Nintendo SNES/Super Mario World',
    expectedStrategy: 'flat_game_collection',
    expectedTitle: 'Super Mario World OST - Koji Kondo (SPC)',
    expectedFormat: 'SPC',
  },
  {
    path: 'Piano E-Competition MIDI/2009/Adam Zukiewicz',
    expectedStrategy: 'compo_event',
    expectedTitle: 'Piano E-Competition 2009 · Adam Zukiewicz (MIDI)',
    expectedFormat: 'MIDI',
  },
  {
    path: 'Battle of the Bits/FAMICOM GRANDPRIX [BotB 9038] (2024)',
    expectedStrategy: 'compo_event',
    expectedTitle: 'FAMICOM GRANDPRIX [BotB 9038] (2024) · BotB (NSFE)',
    expectedFormat: 'NSFE',
  },
  {
    path: 'Classical MIDI/Bach/Bwv988 The Goldberg Variations',
    expectedStrategy: 'artist_discography',
    expectedTitle: 'Bwv988 The Goldberg Variations · Bach (MIDI)',
    expectedFormat: 'MIDI',
  },
  {
    path: 'Sound Canvas MIDI Collection/Key/Clannad/cla_Negai2',
    expectedStrategy: 'deep_thematic_archive',
    expectedTitle: 'cla_Negai2 · Clannad (MIDI)',
    expectedFormat: 'MIDI',
  },
  {
    path: 'Sharp X68000 MDX/Game/FillInCafe/NeuralGear',
    expectedStrategy: 'deep_thematic_archive',
    expectedTitle: 'NeuralGear · FillInCafe (MDX)',
    expectedFormat: 'MDX',
  },
];

let passed = 0;
let total = 0;

structuralTestCases.forEach(({ path: testPath, expectedStrategy, expectedTitle, expectedFormat }) => {
  total++;
  const topPrefix = testPath.split('/')[0];
  const strategy = PREFIX_STRUCTURE_MAP[topPrefix] || 'default';
  assert.strictEqual(strategy, expectedStrategy, `Strategy for ${topPrefix} should be ${expectedStrategy}`);

  const meta = resolveDirectoryMetadata(testPath);

  // Title checks
  assert(meta.title, `Title should exist for ${testPath}`);
  assert(!meta.title.includes('Chip Player JS'), `Title should NOT contain 'Chip Player JS': "${meta.title}"`);
  assert(meta.title.length <= 60, `Title length (${meta.title.length}) exceeds 60 chars: "${meta.title}"`);
  if (expectedTitle) {
    assert.strictEqual(meta.title, expectedTitle, `Expected title "${expectedTitle}" but got "${meta.title}"`);
  }

  // Format checks
  if (expectedFormat) {
    assert(meta.formats.includes(expectedFormat), `Formats for ${testPath} should include ${expectedFormat}: [${meta.formats}]`);
    assert(meta.title.includes(`(${expectedFormat}`) || meta.title.includes(expectedFormat), `Title should include format: "${meta.title}"`);
  }

  // Description checks
  assert(meta.description, `Description should exist for ${testPath}`);
  assert(meta.description.length > 20, `Description is too short: "${meta.description}"`);

  // Schema.org JSON-LD checks
  assert(meta.schemaJsonLd, `Schema JSON-LD should exist for ${testPath}`);
  const jsonMatch = meta.schemaJsonLd.match(/<script type="application\/ld\+json">(.*?)<\/script>/);
  assert(jsonMatch, `Schema should be inside <script> tag: ${meta.schemaJsonLd}`);
  const parsedSchema = JSON.parse(jsonMatch[1]);
  assert(Array.isArray(parsedSchema), 'Schema should be an array with BreadcrumbList & CollectionPage');
  assert.strictEqual(parsedSchema[0]['@type'], 'BreadcrumbList');
  assert.strictEqual(parsedSchema[1]['@type'], 'CollectionPage');

  // Noscript checks
  assert(meta.noscriptContent, `Noscript content should exist for ${testPath}`);
  assert(meta.noscriptContent.includes('seo-crawler-content'), `Noscript should contain crawler container`);

  console.log(`✓ [STRUCTURAL:${strategy}] ${testPath}`);
  console.log(`  Title: "${meta.title}" (${meta.title.length} chars)`);
  console.log(`  Formats: [${meta.formats.join(', ')}]`);
  console.log();
  passed++;
});

console.log(`Structural tests passed: ${passed}/${total}\n`);

// 2. SIMPLIFIED PATH STRATEGY TESTS
console.log('Testing Simplified Path Strategy Mode...');
setStrategyMode('path');
assert.strictEqual(getStrategyMode(), 'path');

const pathTestCases = [
  {
    path: 'Famicompo/mini5/Cover',
    expectedTitle: 'Famicompo / mini5 / Cover (NSF)',
  },
  {
    path: 'Nintendo SNES/Super Mario World',
    expectedTitle: 'Nintendo SNES / Super Mario World (SPC)',
  },
  {
    path: 'Contemporary/James Paddock/Doom Projects/Complete/Jenesis',
    // Must replace middle segments with ellipsis to fit <= 60 chars
    containsEllipsis: true,
  },
  {
    path: 'High Voltage SID Collection/MUSICIANS/T/Tel_Jeroen',
    containsEllipsis: true,
  },
  {
    path: 'Battle of the Bits/FAMICOM GRANDPRIX [BotB 9038] (2024)',
    // 2 segments: must NOT have middle ellipsis, must start with "Battle of the Bits / "
    noMiddleEllipsis: true,
  },
];

pathTestCases.forEach(({ path: testPath, expectedTitle, containsEllipsis, noMiddleEllipsis }) => {
  const meta = resolveDirectoryMetadata(testPath);
  assert(meta.title.length <= 60, `Path title exceeds 60 chars: "${meta.title}"`);
  if (expectedTitle) {
    assert.strictEqual(meta.title, expectedTitle);
  }
  if (containsEllipsis) {
    assert(meta.title.includes('…'), `Path title should contain ellipsis: "${meta.title}"`);
  }
  if (noMiddleEllipsis) {
    assert(!meta.title.includes(' / … / '), `2-segment path should NOT contain middle ellipsis: "${meta.title}"`);
  }
  console.log(`✓ [PATH] ${testPath} -> "${meta.title}" (${meta.title.length} chars)`);
});

// Test helper directly
const longPathSegments = ['Contemporary', 'James Paddock', 'Doom Projects', 'Complete', 'Jenesis'];
const truncatedPathTitle = formatSimplifiedPathTitle(longPathSegments, 60, { includeFormat: true, formats: ['MIDI'] });
assert(truncatedPathTitle.includes('…'), 'Long path should be truncated with middle ellipsis');
assert(truncatedPathTitle.length <= 60, 'Truncated path title should be <= 60 characters');
console.log(`✓ Middle ellipsis truncation helper verified: "${truncatedPathTitle}"`);

// Verify 2-segment behavior: AAA / BBB -> AAA / B...
const twoSeg = formatSimplifiedPathTitle(['AAA_Long_Name', 'BBB_Very_Long_Soundtrack_Title_That_Exceeds_The_Budget'], 40);
assert(!twoSeg.includes(' / … / '), '2-segment title must not have middle ellipsis');
assert(twoSeg.startsWith('AAA_Long_Name / '), '2-segment title must start with first segment');
assert(twoSeg.endsWith('…'), '2-segment title must end with ellipsis on leaf');
console.log(`✓ 2-segment end-truncation verified: "${twoSeg}"`);

// Verify 4-segment progressive removal from N - 1:
const fourSeg = ['AAA', 'BBB', 'CCC', 'DDD_Soundtrack_Title'];
const step1 = formatSimplifiedPathTitle(fourSeg, 37);
assert.strictEqual(step1, 'AAA / BBB / … / DDD_Soundtrack_Title', 'Should remove CCC first');
const step2 = formatSimplifiedPathTitle(fourSeg, 30);
assert.strictEqual(step2, 'AAA / … / DDD_Soundtrack_Title', 'Should remove both BBB and CCC');
const step3 = formatSimplifiedPathTitle(fourSeg, 20);
assert.strictEqual(step3, 'AAA / … / DDD_Sound…', 'Should truncate leaf when all middle removed');
console.log(`✓ 4-segment progressive N-1 removal verified: "${step1}" -> "${step2}" -> "${step3}"`);

// Switch back to default mode
setStrategyMode('path');
assert.strictEqual(getStrategyMode(), 'path');
console.log('✓ Dynamic mode switching verified (structural <-> path)\n');

console.log('All SEO tests passed successfully!');
