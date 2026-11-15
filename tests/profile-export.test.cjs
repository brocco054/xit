const assert = require('node:assert/strict');
const { buildInternalProfile, buildProfileReview } = require('../ui/profile-export.js');

const preset = (weapon_definition_id, paint_kit_id, seed, wear) => ({
  weapon_definition_id, paint_kit_id, seed, wear,
});

const ak = preset(7, 44, 857, 0.24);
const knife = preset(507, 568, 42, 0.02);
const glove = preset(5030, 10037, 1, 0.18);
const profile = buildInternalProfile([ak, knife, glove, preset(7, 724, 20, 0.4)], ak);
assert.equal(profile.skin_changer.weapon_7_paint_kit_id, 44);
assert.equal(profile.skin_changer.weapon_7_seed, 857);
assert.equal(profile.skin_changer.weapon_7_wear, 0.24);
assert.equal(profile.knife_changer.knife_id, 507);
assert.equal(profile.knife_changer.paint_kit_id, 568);
assert.equal(profile.glove_changer.glove_id, 5030);
assert.equal(profile.glove_changer.paint_kit_id, 10037);
assert.equal(buildInternalProfile([], ak).knife_changer.enabled, false);
assert.equal(buildInternalProfile([], knife).skin_changer.enabled, false);
assert.equal(buildInternalProfile([], glove).glove_changer.enabled, true);
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
const picked = review.filter(({ editor, selected }) => !editor && selected).map(({ selected }) => selected.preset);
assert.equal(buildInternalProfile(picked, glock).skin_changer.weapon_7_paint_kit_id, 44);
assert.equal(buildProfileReview(variants, preferred, lotus).find(({ slot }) => slot === 'weapon-7').editor, lotus);
console.log('15 profile export checks passed');
