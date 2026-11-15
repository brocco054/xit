const { writeFileSync } = require('node:fs');
const { buildInternalProfile } = require('../ui/profile-export.js');

const destination = process.argv[2];
if (!destination) throw new Error('Informe o caminho do perfil de teste.');

const rifle = { weapon_definition_id: 7, paint_kit_id: 44, seed: 857, wear: 0.24 };
const knife = { weapon_definition_id: 507, paint_kit_id: 568, seed: 42, wear: 0.02 };
const glove = { weapon_definition_id: 5030, paint_kit_id: 10037, seed: 19, wear: 0.18 };
writeFileSync(destination, JSON.stringify(buildInternalProfile([knife, glove], rifle), null, 2));
