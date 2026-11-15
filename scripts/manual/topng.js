// node topng.js in.svg out.png : render an SVG at 4 px/mm (about 2x screen size)
const { chromium } = require('playwright');
(async () => {
  const [inp, out] = process.argv.slice(2);
  const b = await chromium.launch();
  const p = await b.newPage({ deviceScaleFactor: 1 });
  await p.goto('file://' + require('path').resolve(inp));
  const svg = await p.$('svg');
  await p.evaluate(() => { const s = document.querySelector('svg'); const w = parseFloat(s.getAttribute('width')), h = parseFloat(s.getAttribute('height')); s.setAttribute('width', (w * 6) + 'px'); s.setAttribute('height', (h * 6) + 'px'); });
  await svg.screenshot({ path: out, omitBackground: true });
  await b.close();
})();
