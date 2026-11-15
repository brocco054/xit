function profileSlot(definition) {
  if (definition >= 5000) return 'glove';
  if (definition >= 500) return 'knife';
  return `weapon-${definition}`;
}

function buildProfileReview(records, choices, current) {
  const editorSlot = profileSlot(current.weapon_definition_id);
  const grouped = new Map([[editorSlot, []]]);
  for (const record of records) {
    const slot = profileSlot(record.preset.weapon_definition_id);
    if (!grouped.has(slot)) grouped.set(slot, []);
    grouped.get(slot).push(record);
  }
  return [...grouped.keys()].sort().map((slot) => {
    const candidates = grouped.get(slot).sort((a, b) => a.id < b.id ? -1 : Number(a.id !== b.id));
    const editor = slot === editorSlot ? current : null;
    const selected = editor ? null
      : candidates.find(({ id }) => id === choices[slot]) ?? (candidates.length === 1 ? candidates[0] : null);
    return { slot, candidates, editor, selected };
  });
}

function buildInternalProfile(savedPresets, current) {
  const profile = {
    skin_changer: { enabled: false, selected_weapon_id: 0 },
    knife_changer: { enabled: false, knife_id: 0, paint_kit_id: 0 },
    glove_changer: { enabled: false, glove_id: 0, paint_kit_id: 0 },
  };

  for (let definition = 1; definition <= 70; definition++) {
    const slot = `weapon_${definition}`;
    Object.assign(profile.skin_changer, {
      [`${slot}_paint_kit_id`]: 0,
      [`${slot}_seed`]: 0,
      [`${slot}_wear`]: 0.0001,
      [`${slot}_custom_name`]: '',
    });
  }

  // One finish per weapon and one selection each for knives and gloves.
  // The current editor selection wins if a slot is already populated.
  for (const preset of [...savedPresets, current]) {
    const { weapon_definition_id: definition, paint_kit_id: kit, seed, wear } = preset;
    if (definition >= 5000) {
      profile.glove_changer = { enabled: true, glove_id: definition, paint_kit_id: kit, seed, wear };
    } else if (definition >= 500) {
      profile.knife_changer = { enabled: true, knife_id: definition, paint_kit_id: kit, seed, wear };
    } else {
      const slot = `weapon_${definition}`;
      Object.assign(profile.skin_changer, {
        enabled: true,
        selected_weapon_id: definition,
        [`${slot}_paint_kit_id`]: kit,
        [`${slot}_seed`]: seed,
        [`${slot}_wear`]: wear,
      });
    }
  }
  return profile;
}

if (typeof module !== 'undefined') module.exports = { buildInternalProfile, buildProfileReview, profileSlot };
