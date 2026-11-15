const assert = require('node:assert/strict');
const { buildCollectionExport, buildProfileReview } = require('../ui/profile-export.js');

const preset = (weapon_definition_id, paint_kit_id, seed, wear) => ({ weapon_definition_id, paint_kit_id, seed, wear });
const catalog = [
  { weaponId: 7, kit: 44, weapon: 'AK-47', name: 'Case Hardened', min: 0, max: 1, minSeed: 1, maxSeed: 1000 },
  { weaponId: 7, kit: 724, weapon: 'AK-47', name: 'Wild Lotus', min: 0, max: 1, minSeed: 1, maxSeed: 1000 },
  { weaponId: 507, kit: 568, weapon: 'Karambit', name: 'Gamma Doppler - Emerald', min: 0, max: 0.08, minSeed: 1, maxSeed: 1000 },
  { weaponId: 5030, kit: 10037, weapon: 'Sport Gloves', name: "Pandora's Box", min: 0.06, max: 0.8, minSeed: 1, maxSeed: 1000 },
  { weaponId: 4, kit: 38, weapon: 'Glock-18', name: 'Fade', min: 0, max: 0.08, minSeed: 1, maxSeed: 1000 },
];
const ak = preset(7, 44, 857, 0.24);
const knife = preset(507, 568, 42, 0.02);
const glove = preset(5030, 10037, 1, 0.18);
const saved = [
  { id: 'knife-preset', name: 'Karambit Emerald', preset: knife },
  { id: 'glove-preset', name: 'Pandora Box', preset: glove },
];
const collection = buildCollectionExport(saved, ak, 'AK Case Hardened', catalog);
assert.equal(collection.format, 'hy3x.acervo.collection');
assert.equal(collection.version, 1);
assert.equal(collection.items.length, 3);
assert.equal(collection.items[0].weapon, 'AK-47');
assert.equal(collection.items[0].weapon_definition_id, 7);
assert.equal(collection.items[0].finish, 'Case Hardened');
assert.equal(collection.items[0].paint_kit_id, 44);
assert.equal(collection.items[0].seed, 857);
assert.equal(collection.items[0].wear, 0.24);
assert.equal(collection.items[0].name, 'AK Case Hardened');
assert.equal(collection.items[1].weapon, 'Karambit');
assert.equal(collection.items[2].weapon, 'Sport Gloves');
assert.equal('skin_changer' in collection, false);
assert.equal('knife_changer' in collection, false);
assert.equal('glove_changer' in collection, false);
assert.equal('enabled' in collection.items[0], false);
assert.throws(() => buildCollectionExport([], preset(7, 999, 1, 0.2), 'Invalid', catalog), /não existe/);
assert.throws(() => buildCollectionExport([], preset(7, 44, 1001, 0.2), 'Invalid', catalog), /fora dos limites/);
assert.throws(() => buildCollectionExport([saved[0]], knife, 'Duplicado', catalog), /mesmo slot/);

const lotus = preset(7, 724, 20, 0.4);
const glock = preset(4, 38, 1, 0.08);
const variants = [
  { id: 'lotus', name: 'Wild Lotus', preset: lotus },
  { id: 'case', name: 'Case Hardened', preset: ak },
];
const preferred = { 'weapon-7': 'case' };
const review = buildProfileReview(variants, preferred, glock);
assert.deepEqual(review, buildProfileReview([...variants].reverse(), preferred, glock));
assert.equal(review.find(({ slot }) => slot === 'weapon-7').selected.id, 'case');
assert.equal(buildProfileReview(variants, {}, glock).find(({ slot }) => slot === 'weapon-7').selected, null);
assert.equal(buildProfileReview(variants, preferred, lotus).find(({ slot }) => slot === 'weapon-7').editor, lotus);
console.log('20 portable collection export checks passed');