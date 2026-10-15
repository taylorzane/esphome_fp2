// Serve the repository root in another terminal:
// uv run --locked python -m http.server 8765 --bind 127.0.0.1
// Then use an isolated Playwright CLI browser session:
// bunx --bun @playwright/cli -s=fp2-layout open https://demo.home-assistant.io
// bunx --bun @playwright/cli -s=fp2-layout run-code "$(cat tests/sections-layout.browser.js)"
// Uses the real Sections renderer with an in-memory fixture; never saves a
// dashboard or calls a device service. Requires internet access to the HA demo.
async (page) => {
  await page.goto('https://demo.home-assistant.io/#/lovelace/home');
  await page.waitForFunction(() => {
    const find = root => [...root.querySelectorAll('*')].some(element =>
      element.localName === 'hui-section' || (element.shadowRoot && find(element.shadowRoot)));
    return find(document);
  });
  const source = await page.request.get('http://127.0.0.1:8765/card.js');
  if (!source.ok()) throw new Error('Cannot load local card.js');
  await page.addScriptTag({content: await source.text()});
  await page.evaluate(() => {
    window.findFP2TestElement = (root, name) => {
      for (const element of root.querySelectorAll('*')) {
        if (element.localName === name) return element;
        if (element.shadowRoot) {
          const found = window.findFP2TestElement(element.shadowRoot, name);
          if (found) return found;
        }
      }
    };
    const Card = customElements.get('aqara-fp2-card');
    Card.prototype.fetchMapConfig = async function () {
      this.mapConfig = {mounting_position:'wall', left_right_reverse:false, zones:[], global_sensitivity:2};
      this.updateCard();
      return true;
    };
    window.findFP2TestElement(document, 'hui-section').config = {
      type:'grid', cards:[
        {type:'custom:aqara-fp2-card', entity_prefix:'sensor.fp2_layout', title:'FP2 layout and existing zone editor'},
        {type:'markdown', content:'Following card: layout boundary marker'},
      ],
    };
  });
  await page.locator('aqara-fp2-card canvas').waitFor();
  const results = [];
  for (const [width,height] of [[1440,1000],[1440,500],[390,844],[320,568],[844,390]]) {
    await page.setViewportSize({width,height});
    for (const editing of [false,true]) {
      await page.evaluate(editing => {
        const card = window.findFP2TestElement(document, 'aqara-fp2-card');
        if (card.editing !== editing) card.querySelector('.edit-toggle').click();
      }, editing);
      await page.waitForTimeout(300);
      const result = await page.evaluate(() => {
        const find = window.findFP2TestElement;
        const card = find(document,'aqara-fp2-card');
        const next = find(find(document,'hui-section'),'hui-markdown-card');
        const bounds = card.getBoundingClientRect(), canvas = card.canvas.getBoundingClientRect();
        return {
          width:innerWidth, height:innerHeight, editing:card.editing,
          followingBelow:next.getBoundingClientRect().top >= bounds.bottom,
          noOverflow:card.scrollWidth <= card.clientWidth + 1,
          canvasFits:canvas.width <= card.content.clientWidth + 1,
          heightCapped:canvas.height <= (window.visualViewport?.height || innerHeight) * .65 + 1,
          square:Math.abs(canvas.width - canvas.height) < 1,
        };
      });
      if (['followingBelow','noOverflow','canvasFits','heightCapped','square'].some(key => !result[key])) {
        throw new Error(JSON.stringify(result));
      }
      results.push(result);
    }
    await page.locator('aqara-fp2-card canvas').scrollIntoViewIfNeeded();
    const points = await page.evaluate(() => {
      const card = window.findFP2TestElement(document,'aqara-fp2-card');
      card.editorState[card.selectedLayer].forEach(row => row.fill(0));
      const bounds = card.canvas.getBoundingClientRect();
      return [2,3,13].map(x => ({
        x:bounds.left + card.canvas.clientLeft + (x+.5)*card.canvas.clientWidth/14,
        y:bounds.top + card.canvas.clientTop + 4.5*card.canvas.clientHeight/14,
      }));
    });
    await page.mouse.move(points[0].x,points[0].y);
    await page.mouse.down();
    await page.mouse.move(points[1].x,points[1].y);
    await page.mouse.up();
    await page.mouse.click(points[2].x,points[2].y);
    const painted = await page.evaluate(() => {
      const card = window.findFP2TestElement(document,'aqara-fp2-card');
      return [2,3,13].every(x => card.editorState[card.selectedLayer][4][x] === 1);
    });
    if (!painted) throw new Error(`Painting failed at ${width}x${height}`);
  }
  return {layouts:results, painting:'passed at every viewport'};
}
