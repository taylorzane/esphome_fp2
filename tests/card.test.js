import { beforeAll, test, expect } from 'bun:test';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';
let Card;
beforeAll(() => {
  const context = { window: { customCards: [] }, HTMLElement: class {}, console,
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
