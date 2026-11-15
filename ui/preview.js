function simulatesPattern(finish) {
  return /fade|doppler|case hardened|crimson web/i.test(finish.name);
}

function createPreview(canvas, message) {
  const context = canvas.getContext('2d');
  const setError = (text) => {
    message.textContent = text;
    message.hidden = !text;
    canvas.hidden = Boolean(text);
  };
  if (!context) {
    setError('Prévia indisponível neste navegador.');
    return { select() {}, render() {} };
  }
  const grainLayer = document.createElement('canvas');
  const grainContext = grainLayer.getContext('2d');
  let grainSettings;
  let source;
  let sourceUrl;
  let request;

  function randomSequence(seed) {
    let state = seed >>> 0;
    return () => {
      state = (Math.imul(state, 1664525) + 1013904223) >>> 0;
      return state / 4294967296;
    };
  }

  function draw({ finish, wear, seed, original }) {
    const { width, height } = canvas;
    context.clearRect(0, 0, width, height);
    if (!source?.complete || !source.naturalWidth) return;

    const scale = Math.min(width / source.naturalWidth, height / source.naturalHeight);
    const imageWidth = source.naturalWidth * scale;
    const imageHeight = source.naturalHeight * scale;
    context.drawImage(source, (width - imageWidth) / 2, (height - imageHeight) / 2, imageWidth, imageHeight);
    if (original) return;

    context.save();
    context.globalCompositeOperation = 'source-atop';

    // This glaze represents an illustrative pattern variation, not an engine material.
    if (simulatesPattern(finish)) {
      const pattern = randomSequence(seed);
      const color = /gamma/i.test(finish.name) ? 155 : /crimson/i.test(finish.name) ? 350 : 245;
      const x = width * (0.25 + pattern() * 0.48);
      const glow = context.createRadialGradient(x, height * 0.47, 0, x, height * 0.47, width * 0.36);
      glow.addColorStop(0, `hsla(${color + pattern() * 65}, 70%, 58%, 0.45)`);
      glow.addColorStop(1, `hsla(${color}, 55%, 47%, 0)`);
      context.fillStyle = glow;
      context.fillRect(0, 0, width, height);
    }

    if (!grainContext) {
      drawGrain(context, finish, wear, width, height);
    } else {
      if (!grainSettings || grainSettings.finish !== finish || grainSettings.wear !== wear
        || grainLayer.width !== width || grainLayer.height !== height) {
        grainLayer.width = width;
        grainLayer.height = height;
        drawGrain(grainContext, finish, wear, width, height);
        grainSettings = { finish, wear };
      }
      context.drawImage(grainLayer, 0, 0);
    }
    context.restore();
  }

  function drawGrain(target, finish, wear, width, height) {
    const grain = randomSequence(finish.kit);
    const intensity = Math.min(1, Math.max(0, wear));
    const visibleRange = finish.max > finish.min ? (wear - finish.min) / (finish.max - finish.min) : 0;
    const abrasion = Math.max(0, Math.min(1, intensity * 0.72 + visibleRange * 0.18));
    target.fillStyle = `rgba(39, 33, 28, ${abrasion * 0.27})`;
    target.fillRect(0, 0, width, height);
    for (let index = 0; index < Math.round(abrasion * 3800); index++) {
      const x = width * grain();
      const y = height * grain();
      const length = 0.7 + grain() * 5;
      target.fillStyle = index % 4 === 0 ? '#d0cbc280' : '#2a211da0';
      target.fillRect(x, y, length, 0.7 + grain() * 1.2);
    }
  }

  let settings;
  function render(next) {
    settings = next;
    if (request) return;
    request = requestAnimationFrame(() => {
      request = null;
      draw(settings);
    });
  }

  function select(url) {
    if (sourceUrl === url) return;
    sourceUrl = url;
    const image = new Image();
    source = image;
    setError('');
    context.clearRect(0, 0, canvas.width, canvas.height);
    image.addEventListener('load', () => {
      if (source === image && settings) render(settings);
    });
    image.addEventListener('error', () => {
      if (source === image) setError('Imagem de referência indisponível.');
    });
    image.src = url;
  }

  return { select, render };
}
