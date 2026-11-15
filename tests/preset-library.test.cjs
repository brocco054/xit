const assert = require('node:assert/strict');
const { buildProfileReview } = require('../ui/profile-export.js');
const { decodeLibrary, mergeLibrary, migrateLibrary, normalizeSearch } = require('../ui/preset-library.js');

const catalog = [
  { id: 'case-hardened', weaponId: 7, kit: 44, min: 0, max: 1, name: 'Case Hardened' },
  { id: '7-724', weaponId: 7, kit: 724, min: 0, max: 1, name: 'Wild Lotus' },
];
const makePreset = (weapon_definition_id, paint_kit_id, seed, wear) => ({ weapon_definition_id, paint_kit_id, seed, wear });
const legacy = { '7-724': makePreset(7, 724, 4, 0.11), 'case-hardened': makePreset(7, 44, 857, 0.24) };
const initial = migrateLibrary(legacy, catalog);
assert.equal(initial.presets.length, 2);
assert.deepEqual(initial, migrateLibrary(Object.fromEntries(Object.entries(legacy).reverse()), catalog));
const stored = { ...initial, slots: { 'weapon-7': initial.presets[1].id } };
const restored = decodeLibrary(JSON.parse(JSON.stringify(stored)), catalog);
assert.deepEqual(restored, stored);
assert.equal(buildProfileReview(restored.presets, restored.slots, makePreset(4, 38, 1, 0.1))[1].selected.preset.paint_kit_id, 724);
assert.throws(() => decodeLibrary({ ...stored, version: 99 }, catalog), /versão/);
assert.throws(() => decodeLibrary({ ...stored, presets: [{ ...stored.presets[0], preset: { ...legacy['case-hardened'], seed: -1 } }] }, catalog), /inválido/);
const next = { ...initial, presets: initial.presets.map((record) => ({ ...record, name: record.name + ' B' })) };
let sequence = 1;
const merged = mergeLibrary(stored, next, () => 'variant-' + (sequence++));
assert.equal(merged.presets.length, 4);
assert.deepEqual(merged.slots, stored.slots);
assert.deepEqual(mergeLibrary(stored, initial, () => assert.fail()), stored);
assert.equal(normalizeSearch(' AK-47 | Jörmungandr  '), 'ak 47 jormungandr');
console.log('10 preset library checks passed');
