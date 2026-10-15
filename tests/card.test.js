import { beforeAll, test, expect } from 'bun:test';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';
let Card;
beforeAll(() => {
  const context = { window: { customCards: [], innerHeight: 800, devicePixelRatio: 1 }, HTMLElement: class {}, console,
    customElements: { define(_name, klass) { Card = klass; } },
    setTimeout: (fn) => { fn(); return 0; }, clearTimeout() {}, ResizeObserver: class {} };
  vm.runInNewContext(readFileSync('card.js', 'utf8'), context);
});
function cardWithResponses(responses) {
  const card = new Card();
  card.fetchMapConfig = async () => { card.mapConfig = responses.shift() ?? card.mapConfig; };
  return card;
}
test('global save must wait for a new completed durable generation, not just a service call', async () => {
  const card = cardWithResponses([
    {save_status:{generation:1,pending:false,ok:true}},
    {save_status:{generation:2,pending:true,ok:false},global_sensitivity:3},
    {save_status:{generation:2,pending:false,ok:false,error:'flash failed'},global_sensitivity:3},
  ]);
  const result = await card.saveGlobalZoneToSensor({callService:async()=>{}}, 'fp2', 3);
  expect(result.success).toBe(false);
});
test('identical desired geometry from an earlier save does not confirm a new request', async () => {
  const card = cardWithResponses([{save_status:{generation:4,pending:false,ok:true},zones:[{id:1,grid:'0'.repeat(56),sensitivity:2}]}]);
  const result = await card.waitForZoneConfig(1,'0'.repeat(80),2,-1,2,4);
  expect(result).toBe(false);
});
test('completed save succeeds once its matching geometry and new status arrive', async () => {
  const card = cardWithResponses([{save_status:{generation:5,pending:false,ok:true},zones:[{id:1,grid:'0'.repeat(56),sensitivity:2}]}]);
  expect(await card.waitForZoneConfig(1,'0'.repeat(80),2,-1,2,4)).toBe(true);
});
test('Sections sizing follows content and has no fixed row ceiling', () => {
  const options = new Card().getGridOptions();
  expect(options.rows).toBe('auto');
  expect(options.max_rows).toBeUndefined();
});
test('canvas preserves square cells when viewport height caps a wide card', () => {
  const card = new Card();
  card.content = {clientWidth: 800};
  card.canvas = {style: {}};
  card.ctx = new Proxy({}, {get: () => () => {}});
  card.gridSize = 14;
  for (const method of ['drawBaseGrid','drawEdgeLabels','drawInterferenceSources','drawEntryExitZones','drawDetectionZones','drawTargets','drawSensorPosition','drawHoverPreview','drawSelectedLayerOutline']) card[method] = () => {};
  card.renderCanvas({edgeLabelGrid:Array.from({length:14},()=>Array(14).fill(0)),leftRightReverse:false,zones:[],targets:[]});
  expect(card.canvas.width).toBeLessThanOrEqual(520);
  expect(card.canvas.width).toBe(card.canvas.height);
  expect(card.renderParams.cellSize * 14).toBeCloseTo(card.canvas.width);
});
test('painting maps the bordered, resized canvas to its original grid coordinates', () => {
  const card = new Card();
  card.renderParams = {minX:0,minY:0,cellSize:20,canvasWidth:280,canvasHeight:280};
  card.canvas = {clientLeft:2,clientTop:2,clientWidth:266,clientHeight:266,
    getBoundingClientRect:()=>({left:10,top:30})};
  for (let y=0;y<14;y++) for (let x=0;x<14;x++) {
    const cell=card.canvasEventToGridCell({clientX:12+(x+.5)*19,clientY:32+(y+.5)*19});
    expect(cell.x).toBe(x);
    expect(cell.y).toBe(y);
  }
});
