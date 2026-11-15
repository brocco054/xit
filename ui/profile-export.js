function profileSlot(definition) {
  if (definition >= 5000) return 'glove';
  if (definition >= 500) return 'knife';
  return 'weapon-' + definition;
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

/** Build a portable collection document; this is not the native DLL config format. */
function buildCollectionExport(savedPresets, current, currentName, catalog) {
  const entries = [
    ...savedPresets.map((record) => ({ id: record.id, name: record.name, preset: record.preset })),
    { id: null, name: String(currentName ?? '').trim() || 'Seleção atual', preset: current },
  ];
  const seenSlots = new Set();
  const items = entries.map((entry) => {
    const preset = entry.preset;
    const finish = catalog.find((item) =>
      item.weaponId === preset?.weapon_definition_id && item.kit === preset?.paint_kit_id);
    if (!finish) throw new Error('O acabamento selecionado não existe no catálogo atual.');
    const slot = profileSlot(finish.weaponId);
    if (seenSlots.has(slot)) throw new Error('A exportação contém mais de um item para o mesmo slot.');
    seenSlots.add(slot);
    const seedMin = finish.minSeed ?? 1;
    const seedMax = finish.maxSeed ?? 1000;
    if (!Number.isInteger(preset.seed) || preset.seed < seedMin || preset.seed > seedMax
      || !Number.isFinite(preset.wear) || preset.wear < finish.min || preset.wear > finish.max) {
      throw new Error('O preset contém seed ou desgaste fora dos limites do catálogo.');
    }
    return {
      preset_id: typeof entry.id === 'string' ? entry.id : null,
      name: String(entry.name ?? 'Preset').trim().slice(0, 80) || 'Preset',
      slot,
      weapon: finish.weapon,
      weapon_definition_id: finish.weaponId,
      finish: finish.name,
      paint_kit_id: finish.kit,
      seed: preset.seed,
      wear: preset.wear,
    };
  });
  items.sort((a, b) => a.weapon_definition_id - b.weapon_definition_id);
  return { format: 'hy3x.acervo.collection', version: 1, source: 'hy3x-acervo-workbench', items };
}

if (typeof module !== 'undefined') {
  module.exports = { buildCollectionExport, buildProfileReview, profileSlot };
}