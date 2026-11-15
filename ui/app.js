const storageKey = 'acervo-presets-v2';
const legacyStorageKey = 'acervo-presets-v1';
const byId = (id) => document.getElementById(id);
const wear = byId('wear');
const exactWear = byId('wear-value');
const seed = byId('seed');
const nameInput = byId('preset-name');
const image = byId('render');
const comparisonImage = byId('compare-render');
const preview = createPreview(image, byId('preview-error'));
const comparisonPreview = createPreview(comparisonImage, byId('compare-error'));
const reference = byId('reference');
const search = byId('search');
const list = byId('items');
const status = byId('status');
const editor = byId('editor');
const libraryTab = byId('library-tab');
const savedTab = byId('saved-tab');
const zoom = byId('zoom');
const dialog = byId('export-dialog');
const exportStatus = byId('export-status');
const backupStatus = byId('backup-status');
const formatWear = new Intl.NumberFormat('pt-BR', { minimumFractionDigits: 4, maximumFractionDigits: 6 });

let selected = finishes.find((finish) => finish.id === 'case-hardened') ?? finishes[0];
let editing = null;
let baseline;
let showSaved = false;
let group = 'Todos';
let original = false;
let comparison = null;
let saved = migrateLibrary({}, finishes);

function finishFor(preset) {
  return finishes.find((finish) => finish.weaponId === preset.weapon_definition_id && finish.kit === preset.paint_kit_id);
}

function readLibrary() {
  try {
    const raw = localStorage.getItem(storageKey);
    if (raw !== null) return decodeLibrary(JSON.parse(raw), finishes);
    return migrateLibrary(JSON.parse(localStorage.getItem(legacyStorageKey) ?? '{}'), finishes);
  } catch {
    throw new Error('A biblioteca armazenada está indisponível. Os dados originais foram preservados.');
  }
}

function writeLibrary(change) {
  const current = readLibrary();
  const next = change(current);
  try {
    localStorage.setItem(storageKey, JSON.stringify(next));
  } catch {
    throw new Error('O navegador não conseguiu salvar a biblioteca.');
  }
  saved = next;
}

function newPresetId() {
  const bytes = crypto.getRandomValues(new Uint8Array(16));
  return Array.from(bytes, (byte) => byte.toString(16).padStart(2, '0')).join('');
}

function wearCondition(value) {
  if (value < 0.07) return 'Nova de fábrica';
  if (value < 0.15) return 'Pouco usada';
  if (value < 0.38) return 'Testada em campo';
  if (value < 0.45) return 'Bem desgastada';
  return 'Veterana de guerra';
}

function presetSummary({ seed: value, wear: level }) {
  return `Seed ${value} · Desgaste ${formatWear.format(level)} · ${wearCondition(level)}`;
}

function currentPreset() {
  return { weapon_definition_id: selected.weaponId, paint_kit_id: selected.kit, seed: seed.valueAsNumber, wear: exactWear.valueAsNumber };
}

function isDirty() {
  return baseline.name !== nameInput.value || baseline.seed !== seed.valueAsNumber || baseline.wear !== exactWear.valueAsNumber;
}

function updateDirty() {
  byId('dirty-status').textContent = isDirty() ? 'Alterações sem salvar' : '';
}

function updateWear(syncExact = true) {
  const value = wear.valueAsNumber;
  const condition = wearCondition(value);
  if (syncExact) exactWear.value = value;
  byId('wear-condition').textContent = condition;
  wear.setAttribute('aria-valuetext', `${formatWear.format(value)}, ${condition}`);
  updatePreview();
}

function updatePreview() {
  const seedValue = seed.validity.valid ? seed.valueAsNumber : Number(seed.min);
  const wearValue = wear.valueAsNumber;
  preview.render({ finish: selected, wear: wearValue, seed: seedValue, original });
  byId('kit-badge').textContent = `${comparison ? 'B · ' : ''}Kit ${selected.kit}`;
  byId('preview-values').textContent = original && !comparison ? '' : presetSummary({ seed: seedValue, wear: wearValue });
}

function showReference(enabled) {
  original = enabled;
  reference.setAttribute('aria-pressed', String(enabled));
  reference.textContent = enabled ? 'Ver simulação' : 'Ver original';
  byId('preview-mode').textContent = enabled ? 'Imagem de referência' : 'Simulação 2D';
  if (comparison) {
    comparisonPreview.render({ finish: comparison.finish, seed: comparison.seed, wear: comparison.wear, original });
    byId('compare-values').textContent = presetSummary(comparison);
  }
  updatePreview();
}

function setZoom(enabled) {
  image.classList.toggle('is-zoomed', enabled);
  comparisonImage.classList.toggle('is-zoomed', enabled);
  zoom.setAttribute('aria-pressed', String(enabled));
  zoom.textContent = enabled ? 'Vista normal' : 'Ampliar';
}

function updateSelection() {
  for (const button of list.querySelectorAll('button')) {
    const active = showSaved ? button.dataset.record === editing?.id : button.dataset.finish === selected.id;
    button.setAttribute('aria-pressed', String(active));
  }
}

function drawList() {
  const focused = list.contains(document.activeElement) ? document.activeElement.dataset : null;
  const tokens = normalizeSearch(search.value).split(' ').filter(Boolean);
  const entries = showSaved
    ? saved.presets.map((record) => ({ finish: finishFor(record.preset), record }))
    : finishes.map((finish) => ({ finish, record: null }));
  const visible = entries.filter(({ finish, record }) => {
    if (group !== 'Todos' && finish.group !== group) return false;
    const term = normalizeSearch(`${finish.weapon} ${finish.name} ${finish.weaponId} ${finish.kit} ${record?.name ?? ''}`);
    return tokens.every((token) => term.includes(token));
  });
  const results = document.createDocumentFragment();
  let nextFocus;
  byId('saved-count').textContent = saved.presets.length;
  byId('result-count').textContent = visible.length === 1 ? '1 resultado' : `${visible.length} resultados`;

  if (!visible.length) {
    const empty = document.createElement('li');
    empty.className = 'empty';
    empty.textContent = showSaved && !saved.presets.length ? 'Salve um preset para vê-lo aqui.' : 'Nenhum resultado encontrado.';
    results.append(empty);
  }

  for (const { finish, record } of visible) {
    const row = document.createElement('li');
    const button = document.createElement('button');
    button.type = 'button';
    button.dataset.finish = finish.id;
    if (record) button.dataset.record = record.id;
    if (focused?.finish === finish.id && focused.record === record?.id) nextFocus = button;
    const thumbnail = document.createElement('img');
    thumbnail.loading = 'lazy';
    thumbnail.decoding = 'async';
    thumbnail.width = 62;
    thumbnail.height = 42;
    thumbnail.src = `assets/${finish.id}.png`;
    thumbnail.alt = '';
    const description = document.createElement('span');
    description.className = 'item-text';
    const name = document.createElement('strong');
    name.textContent = record?.name ?? finish.name;
    const detail = document.createElement('small');
    detail.textContent = record ? `${finish.weapon} · ${finish.name} · Seed ${record.preset.seed}` : finish.weapon;
    description.append(name, detail);
    button.append(thumbnail, description);
    row.append(button);
    results.append(row);
  }
  list.replaceChildren(results);
  updateSelection();
  if (focused) (nextFocus ?? search).focus({ preventScroll: true });
}

function fillEditor(record) {
  const starterWear = selected.min + (selected.max - selected.min) * 0.35;
  const suggestedWear = Math.max(selected.min, Math.min(0.12, starterWear));
  nameInput.value = record?.name ?? selected.name.slice(0, nameInput.maxLength);
  wear.value = record?.preset.wear ?? suggestedWear;
  seed.value = record?.preset.seed ?? seed.min;
  updateWear();
  baseline = { name: nameInput.value, wear: exactWear.valueAsNumber, seed: seed.valueAsNumber };
  updateDirty();
}

function setEditing(record) {
  editing = record;
  byId('saved-actions').hidden = !record;
  byId('save-preset').textContent = record ? 'Salvar alterações' : 'Salvar preset';
  updateSelection();
}

function selectFinish(finish, record = null) {
  selected = finish;
  wear.min = exactWear.min = finish.min;
  wear.max = exactWear.max = finish.max;
  seed.min = finish.minSeed ?? 1;
  seed.max = finish.maxSeed ?? 1000;
  byId('wear-min').textContent = formatWear.format(finish.min);
  byId('wear-max').textContent = formatWear.format(finish.max);
  byId('finish-name').textContent = finish.name;
  byId('weapon-name').textContent = finish.weapon;
  byId('collection').textContent = finish.collection || finish.group;
  const seedRange = `Inteiro de ${seed.min} a ${seed.max}.`;
  byId('seed-hint').textContent = simulatesPattern(finish)
    ? `${seedRange} Variação de padrão aproximada.`
    : `${seedRange} Neste acabamento, o efeito visual depende só do desgaste.`;
  const rarity = byId('rarity');
  rarity.textContent = finish.rarity;
  rarity.style.setProperty('--rarity', finish.color);
  preview.select(`assets/${finish.id}.png`);
  image.setAttribute('aria-label', `${finish.weapon} com acabamento ${finish.name}`);
  status.textContent = '';
  setEditing(record);
  fillEditor(record);
}

function switchTab(savedOnly) {
  showSaved = savedOnly;
  savedTab.setAttribute('aria-pressed', String(savedOnly));
  libraryTab.setAttribute('aria-pressed', String(!savedOnly));
  byId('backup-tools').hidden = !savedOnly;
  drawList();
}

function setGroup(nextGroup) {
  group = nextGroup;
  for (const button of byId('categories').querySelectorAll('button')) {
    button.setAttribute('aria-pressed', String(button.dataset.group === group));
  }
  drawList();
}

function setComparison(enabled) {
  if (enabled) {
    comparison = { ...currentPreset(), finish: selected };
    byId('compare-finish').textContent = `A · ${selected.weapon} · ${selected.name}`;
    comparisonImage.setAttribute('aria-label', `Amostra A: ${selected.weapon} com acabamento ${selected.name}`);
    comparisonPreview.select(`assets/${selected.id}.png`);
  } else {
    comparison = null;
  }
  const button = byId('compare');
  button.setAttribute('aria-pressed', String(enabled));
  button.textContent = enabled ? 'Encerrar A/B' : 'Comparar A/B';
  byId('compare-pane').hidden = !enabled;
  byId('render-grid').classList.toggle('comparing', enabled);
  showReference(original);
}

function downloadJson(data, filename) {
  const url = URL.createObjectURL(new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' }));
  const link = document.createElement('a');
  link.href = url;
  link.download = filename;
  link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

function describePreset(value) {
  const finish = finishFor(value);
  return `${finish.weapon} / ${finish.name} · ${presetSummary(value)}`;
}

function drawReview() {
  const review = buildProfileReview(saved.presets, saved.slots, currentPreset());
  const items = document.createDocumentFragment();
  for (const entry of review) {
    const sample = entry.editor ?? entry.candidates[0].preset;
    const title = document.createElement('strong');
    title.textContent = entry.slot === 'glove' ? 'Luvas' : entry.slot === 'knife' ? 'Faca' : finishFor(sample).weapon;
    const item = document.createElement('li');
    item.append(title);
    if (entry.editor || entry.candidates.length === 1) {
      const details = document.createElement('p');
      details.textContent = describePreset(entry.editor ?? entry.selected.preset);
      item.append(details);
      const source = document.createElement('small');
      if (entry.editor) {
        source.textContent = entry.candidates.length
          ? `Em edição · substitui ${entry.candidates.length} preset(s) neste slot` : 'Em edição';
      } else {
        source.textContent = entry.selected.name;
      }
      item.append(source);
    } else {
      const select = document.createElement('select');
      select.setAttribute('aria-label', `Preset para ${title.textContent}`);
      select.dataset.slot = entry.slot;
      select.append(new Option('Escolha um preset', ''));
      for (const record of entry.candidates) {
        select.append(new Option(`${record.name} · ${describePreset(record.preset)}`, record.id));
      }
      select.value = entry.selected?.id ?? '';
      item.append(select);
    }
    items.append(item);
  }
  byId('export-slots').replaceChildren(items);
  const unresolved = review.some(({ editor, selected }) => !editor && !selected);
  byId('export-confirm').disabled = unresolved;
  exportStatus.textContent = unresolved ? 'Escolha um preset para cada slot com várias opções.' : '';
  return review;
}

function updateRecord(action) {
  try {
    if (!editor.reportValidity()) return;
    const name = nameInput.value.trim();
    if (!name) {
      status.textContent = 'Digite um nome para o preset.';
      nameInput.focus();
      return;
    }
    const record = {
      id: action === 'edit' ? editing.id : newPresetId(),
      name: action === 'copy' ? `${name.slice(0, 72)} (cópia)` : name,
      preset: currentPreset(),
    };
    writeLibrary((current) => {
      if (action === 'edit' && !sameRecord(current.presets.find(({ id }) => id === record.id), editing)) {
        throw new Error('Este preset mudou em outra aba. Abra-o novamente antes de editar.');
      }
      const records = current.presets.map((item) => item.id === record.id ? record : item);
      if (action !== 'edit') records.push(record);
      return { ...current, presets: records };
    });
    setEditing(record);
    fillEditor(record);
    drawList();
    status.textContent = action === 'copy' ? 'Cópia do preset salva.' : 'Preset salvo.';
  } catch (error) {
    status.textContent = error.message;
  }
}

libraryTab.addEventListener('click', () => switchTab(false));
savedTab.addEventListener('click', () => switchTab(true));
byId('categories').addEventListener('click', (event) => {
  const button = event.target.closest('button[data-group]');
  if (button) setGroup(button.dataset.group);
});
search.addEventListener('input', drawList);
list.addEventListener('click', (event) => {
  const button = event.target.closest('button[data-finish]');
  if (!button) return;
  const record = saved.presets.find(({ id }) => id === button.dataset.record);
  const finish = finishes.find(({ id }) => id === button.dataset.finish);
  if (finish === selected && record?.id === editing?.id && !record) return;
  if (finish === selected && record && sameRecord(record, editing)) return;
  if (isDirty() && !confirm('Descartar as alterações deste preset?')) return;
  selectFinish(finish, record);
});
zoom.addEventListener('click', () => setZoom(zoom.getAttribute('aria-pressed') === 'false'));
reference.addEventListener('click', () => showReference(!original));
byId('compare').addEventListener('click', () => {
  if (!comparison && (!seed.reportValidity() || !exactWear.reportValidity())) return;
  setComparison(!comparison);
});
wear.addEventListener('input', () => {
  updateWear();
  updateDirty();
});
exactWear.addEventListener('input', () => {
  if (exactWear.validity.valid) {
    wear.value = exactWear.value;
    updateWear(false);
  }
  updateDirty();
});
seed.addEventListener('input', () => {
  updateDirty();
  if (seed.validity.valid) updatePreview();
});
nameInput.addEventListener('input', updateDirty);
editor.addEventListener('input', () => { status.textContent = ''; });
byId('random').addEventListener('click', () => {
  const min = Number(seed.min);
  const max = Number(seed.max);
  seed.value = min + Math.floor(Math.random() * (max - min + 1));
  updateDirty();
  updatePreview();
  status.textContent = '';
});
byId('reset').addEventListener('click', () => {
  fillEditor(editing);
  status.textContent = editing ? 'Preset salvo restaurado.' : 'Ajustes iniciais restaurados.';
});

editor.addEventListener('submit', (event) => {
  event.preventDefault();
  updateRecord(editing ? 'edit' : 'new');
});
byId('duplicate-preset').addEventListener('click', () => updateRecord('copy'));
byId('delete-preset').addEventListener('click', () => {
  if (!editing || !confirm(`Excluir o preset “${editing.name}”?`)) return;
  try {
    writeLibrary((current) => {
      if (!sameRecord(current.presets.find(({ id }) => id === editing.id), editing)) {
        throw new Error('Este preset mudou em outra aba. Abra-o novamente antes de excluir.');
      }
      return {
        ...current,
        presets: current.presets.filter(({ id }) => id !== editing.id),
        slots: Object.fromEntries(Object.entries(current.slots).filter(([, id]) => id !== editing.id)),
      };
    });
    setEditing(null);
    fillEditor(null);
    drawList();
    byId('save-preset').focus();
    status.textContent = 'Preset excluído.';
  } catch (error) {
    status.textContent = error.message;
  }
});

byId('backup-export').addEventListener('click', () => {
  try {
    downloadJson(readLibrary(), 'acervo-presets.json');
    backupStatus.textContent = 'Backup baixado.';
  } catch (error) {
    backupStatus.textContent = error.message;
  }
});
byId('backup-import').addEventListener('click', () => byId('backup-file').click());
byId('backup-file').addEventListener('change', async (event) => {
  const file = event.target.files[0];
  event.target.value = '';
  if (!file) return;
  try {
    const backup = decodeLibrary(JSON.parse(await file.text()), finishes);
    writeLibrary((current) => mergeLibrary(current, backup, newPresetId));
    drawList();
    backupStatus.textContent = 'Backup incorporado à biblioteca.';
  } catch (error) {
    backupStatus.textContent = error instanceof SyntaxError ? 'Arquivo de backup inválido.' : error.message;
  }
});
window.addEventListener('storage', (event) => {
  if (event.key !== storageKey && event.key !== legacyStorageKey && event.key !== null) return;
  try {
    saved = readLibrary();
    drawList();
    if (dialog.open) {
      drawReview();
      exportStatus.textContent = 'A biblioteca mudou em outra aba. Confira o perfil atualizado.';
      byId('export-cancel').focus();
    }
  } catch (error) {
    status.textContent = error.message;
  }
});

byId('export-profile').addEventListener('click', () => {
  if (!seed.reportValidity() || !exactWear.reportValidity()) return;
  let libraryError;
  try {
    saved = readLibrary();
    drawList();
  } catch (error) {
    libraryError = error.message;
  }
  drawReview();
  if (libraryError) exportStatus.textContent = libraryError;
  dialog.showModal();
});
byId('export-cancel').addEventListener('click', () => dialog.close());
byId('export-slots').addEventListener('change', (event) => {
  const select = event.target.closest('select[data-slot]');
  if (!select) return;
  let errorMessage;
  try {
    writeLibrary((current) => {
      const slots = { ...current.slots };
      if (!select.value) delete slots[select.dataset.slot];
      else if (current.presets.some(({ id, preset }) => id === select.value
        && profileSlot(preset.weapon_definition_id) === select.dataset.slot)) slots[select.dataset.slot] = select.value;
      else throw new Error('Este preset foi removido em outra aba.');
      return { ...current, slots };
    });
  } catch (error) {
    errorMessage = error.message;
  }
  drawReview();
  if (errorMessage) exportStatus.textContent = errorMessage;
  const replacement = Array.from(byId('export-slots').querySelectorAll('select')).find((field) => field.dataset.slot === select.dataset.slot);
  (replacement ?? byId('export-cancel')).focus();
});
byId('export-form').addEventListener('submit', (event) => {
  event.preventDefault();
  try {
    const latest = readLibrary();
    if (JSON.stringify(latest) !== JSON.stringify(saved)) {
      saved = latest;
      drawList();
      drawReview();
      exportStatus.textContent = 'A biblioteca mudou em outra aba. Confira o perfil atualizado.';
      byId('export-cancel').focus();
      return;
    }
  } catch (error) {
    if (saved.presets.length) {
      exportStatus.textContent = error.message;
      return;
    }
  }
  const current = currentPreset();
  const review = buildProfileReview(saved.presets, saved.slots, current);
  if (review.some(({ editor, selected }) => !editor && !selected)) return;
  const chosen = review.filter(({ editor }) => !editor).map(({ selected }) => selected.preset);
  downloadJson(buildInternalProfile(chosen, current), `acervo-${selected.id}.cfg`);
  dialog.close();
  status.textContent = 'Perfil exportado. Carregue o arquivo na aba Perfis do módulo.';
});

selectFinish(selected);
try {
  saved = readLibrary();
} catch (error) {
  status.textContent = error.message;
}
drawList();
