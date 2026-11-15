// Test-only fixture for the retired native profile codec contract.
// This file is deliberately not imported by the standalone companion UI.
const { writeFileSync } = require('node:fs');

const destination = process.argv[2];
if (!destination) throw new Error('Informe o caminho do perfil de teste.');

const fixture = {
  skin_changer: {
    enabled: true,
    selected_weapon: 0,
    selected_weapon_id: 7,
    weapon_7_paint_kit_id: 44,
    weapon_7_seed: 857,
    weapon_7_wear: 0.24,
    weapon_7_custom_name: '',
  },
  knife_changer: {
    enabled: true,
    knife: 2,
    knife_id: 507,
    paint_kit: 2,
    paint_kit_id: 568,
    seed: 42,
    wear: 0.02,
    custom_name: '',
  },
  glove_changer: {
    enabled: true,
    glove: 2,
    glove_id: 5030,
    paint_kit: 1,
    paint_kit_id: 10037,
    seed: 19,
    wear: 0.18,
  },
};

writeFileSync(destination, JSON.stringify(fixture, null, 2) + '\n', { flag: 'wx' });