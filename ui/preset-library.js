function normalizeSearch(value) {
  return value.normalize('NFD').replace(/\p{M}/gu, '').toLowerCase()
    .replace(/[^\p{L}\p{N}]+/gu, ' ').trim();
}

function decodePreset(value, finish) {
  if (!finish || value?.weapon_definition_id !== finish.weaponId || value.paint_kit_id !== finish.kit
    || !Number.isInteger(value.seed) || value.seed < (finish.minSeed ?? 1) || value.seed > (finish.maxSeed ?? 1000)
    || !Number.isFinite(value.wear) || value.wear < finish.min || value.wear > finish.max) return null;
  return { weapon_definition_id: finish.weaponId, paint_kit_id: finish.kit, seed: value.seed, wear: value.wear };
}

function migrateLibrary(value, catalog) {
  const presets = [];
  for (const finish of catalog) {
    const preset = decodePreset(value?.[finish.id], finish);
    if (preset) presets.push({ id: `legacy-${finish.id}`, name: finish.name, preset });
  }
  return { version: 2, presets, slots: {} };
}

function decodeLibrary(value, catalog) {
  if (value?.version !== 2) throw new Error('Formato ou versão da biblioteca incompatível.');
  if (!Array.isArray(value.presets) || !value.slots || typeof value.slots !== 'object' || Array.isArray(value.slots)) {
    throw new Error('Dados da biblioteca inválidos.');
  }
  const entries = new Map();
  const finishesByPair = new Map(catalog.map((finish) => [`${finish.weaponId}/${finish.kit}`, finish]));
  for (const item of value.presets) {
    const { id, name, preset } = item ?? {};
    const finish = finishesByPair.get(`${preset?.weapon_definition_id}/${preset?.paint_kit_id}`);
    const data = decodePreset(preset, finish);
    if (typeof id !== 'string' || !/^[a-z0-9_-]{1,96}$/.test(id) || entries.has(id)
      || typeof name !== 'string' || !name.trim() || name.length > 80 || !data) {
      throw new Error('A biblioteca contém um preset inválido ou incompatível.');
    }
    entries.set(id, { id, name: name.trim(), preset: data });
  }
  for (const [slot, id] of Object.entries(value.slots)) {
    const match = entries.get(id);
    if (!match || slot !== presetProfileSlot(match.preset.weapon_definition_id)) {
      throw new Error('A biblioteca contém uma escolha de slot inválida.');
    }
  }
  return { version: 2, presets: [...entries.values()], slots: { ...value.slots } };
}

function sameRecord(left, right) {
  return JSON.stringify(left) === JSON.stringify(right);
}

function mergeLibrary(current, backup, allocateId) {
  const records = new Map(current.presets.map((record) => [record.id, record]));
  const importedIds = new Map();
  for (const record of backup.presets) {
    const existing = records.get(record.id);
    const id = existing && !sameRecord(existing, record) ? allocateId() : record.id;
    importedIds.set(record.id, id);
    records.set(id, { ...record, id });
  }
  const slots = { ...current.slots };
  for (const [slot, id] of Object.entries(backup.slots)) {
    if (!slots[slot]) slots[slot] = importedIds.get(id);
  }
  return { version: 2, presets: [...records.values()], slots };
}

if (typeof module !== 'undefined') module.exports = { decodeLibrary, mergeLibrary, migrateLibrary, normalizeSearch };
const presetProfileSlot = typeof module === 'undefined' ? profileSlot : require('./profile-export.js').profileSlot;

