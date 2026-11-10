#!/usr/bin/env python3
"""Builds a browser preview from the real frames rendered by esphome/wordclock/wordclock.h.

  ./test/run_tests.sh export > build/scenes.json
  python3 tools/make_preview.py build/scenes.json build/preview.html
"""
import json
import sys

PAGE = r'''<title>Word Clock Preview</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="preconnect" href="https://fonts.gstatic.com">
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Saira+Stencil+One&family=IBM+Plex+Sans:wght@400;600&display=swap">
<style>
/* Layout: the clock as a dark printed front plate on the left, the scene list as a control panel next to it. */
:root {
  color-scheme: dark;
  --bg: #101114; --plate: #050506; --edge: #2a2c33; --fg: #e8e6e1; --muted: #9a9ca6; --off: #1d1e23; --accent: #ffb454;
  --stencil: "Saira Stencil One", "Arial Narrow", Impact, sans-serif;
  --body: "IBM Plex Sans", system-ui, sans-serif;
}
body { background: var(--bg); color: var(--fg); font: 15px/1.5 var(--body); }
.wrap { max-width: 1040px; margin: 0 auto; padding-block: 24px 40px; padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px)); display: grid; grid-template-columns: minmax(0, 1.25fr) minmax(0, 1fr); gap: 28px; align-items: start; }
.stage { position: sticky; top: calc(16px + env(safe-area-inset-top, 0px)); display: grid; gap: 12px; min-width: 0; }
.plate { position: relative; background: var(--plate); border: 1px solid var(--edge); aspect-ratio: 1; max-width: 100%; display: grid; grid-template-columns: repeat(11, 1fr); grid-template-rows: repeat(10, 1fr); padding: 8%; container-type: inline-size; }
.cell { display: grid; place-items: center; font-family: var(--stencil); font-size: 6.1cqw; color: var(--off); line-height: 1; }
.dot { position: absolute; width: 1.6%; aspect-ratio: 1; border-radius: 50%; background: var(--off); }
.bar { display: flex; flex-wrap: wrap; gap: 8px 12px; align-items: center; justify-content: space-between; }
h1 { margin: 0; font: 600 15px/1.3 var(--body); text-wrap: balance; }
button { font: inherit; color: inherit; background: none; border: 1px solid var(--edge); padding: 6px 12px; cursor: pointer; }
button:hover { border-color: var(--muted); }
button:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }
.list { display: grid; gap: 18px; min-width: 0; }
.group h2 { margin: 0 0 6px; font: 600 12px/1.2 var(--body); letter-spacing: .09em; text-transform: uppercase; color: var(--muted); }
.group div { display: flex; flex-wrap: wrap; gap: 6px; }
.scene[aria-pressed="true"] { border-color: var(--accent); color: var(--accent); }
.note { margin: 0; color: var(--muted); font-size: 13px; max-width: 62ch; }
@media (max-width: 720px) { .wrap { grid-template-columns: minmax(0, 1fr); } .stage { position: static; } }
</style>
<div class="wrap">
  <section class="stage">
    <div class="plate" id="plate"></div>
    <div class="bar"><h1 id="name"></h1><button id="pause" type="button">Pause</button></div>
    <p class="note">These are the frames computed by wordclock.h. On real LEDs, gamma correction makes colors more saturated and dark tones weaker.</p>
  </section>
  <nav class="list" id="list" aria-label="Scenes"></nav>
</div>
<script type="application/json" id="scenes">__SCENES__</script>
<script>
const FRONT = ["ESKISTAFÜNF", "ZEHNZWANZIG", "DREIVIERTEL", "VORWIFINACH", "HALBAELFÜNF", "EINSXAMZWEI", "DREIAUJVIER", "SECHSNLACHT", "SIEBENZWÖLF", "ZEHNEUNKUHR"];
const scenes = JSON.parse(document.getElementById("scenes").textContent);
const plate = document.getElementById("plate");
const cells = new Array(114);
for (let row = 0; row < 10; row++) for (let col = 0; col < 11; col++) {
  const el = document.createElement("span");
  el.className = "cell";
  el.textContent = FRONT[row][col];
  plate.appendChild(el);
  cells[row * 11 + (row % 2 === 0 ? col : 10 - col)] = el;
}
[["3%", "3%", "left", "top"], ["3%", "3%", "right", "top"], ["3%", "3%", "right", "bottom"], ["3%", "3%", "left", "bottom"]].forEach((p, i) => {
  const el = document.createElement("span");
  el.className = "dot";
  el.style[p[2]] = p[0];
  el.style[p[3]] = p[1];
  plate.appendChild(el);
  cells[110 + i] = el;
});
const shown = new Array(114).fill("000000");
function draw(frame) {
  for (let i = 0; i < 114; i++) {
    const hex = frame.substr(i * 6, 6);
    if (hex === shown[i]) continue;
    shown[i] = hex;
    const el = cells[i];
    const off = hex === "000000";
    if (i >= 110) { el.style.background = off ? "" : "#" + hex; el.style.boxShadow = off ? "" : "0 0 6px #" + hex; }
    else { el.style.color = off ? "" : "#" + hex; el.style.textShadow = off ? "" : "0 0 .35em #" + hex; }
  }
}
const groups = { "Clock and color sets": n => n.startsWith("Word clock") || n.startsWith("Color set"), "Analog clock": n => n.startsWith("Analog"), "Timer": n => n.startsWith("Timer"), "Animations": n => n.startsWith("Animation ") };
const strip = n => n.replace(/^Color set /, "").replace(/^Animation /, "").replace(/^Timer /, "").replace(" (5 min time-lapse)", "");
const list = document.getElementById("list");
const buttons = {};
let current = null, frameIdx = 0, started = 0, paused = matchMedia("(prefers-reduced-motion: reduce)").matches;
const pauseBtn = document.getElementById("pause");
pauseBtn.textContent = paused ? "Play" : "Pause";
function select(name) {
  current = scenes[name];
  frameIdx = 0;
  started = performance.now();
  document.getElementById("name").textContent = name;
  for (const k in buttons) buttons[k].setAttribute("aria-pressed", k === name);
  draw(current.frames[0]);
}
for (const [title, test] of Object.entries(groups)) {
  const box = document.createElement("section");
  box.className = "group";
  box.innerHTML = "<h2></h2><div></div>";
  box.firstChild.textContent = title;
  for (const name of Object.keys(scenes).filter(test)) {
    const b = document.createElement("button");
    b.type = "button";
    b.className = "scene";
    b.textContent = strip(name);
    b.onclick = () => select(name);
    buttons[name] = b;
    box.lastChild.appendChild(b);
  }
  list.appendChild(box);
}
pauseBtn.onclick = () => { paused = !paused; pauseBtn.textContent = paused ? "Play" : "Pause"; started = performance.now() - frameIdx * 1000 / current.fps; };
function tick(now) {
  if (!paused && current) {
    const i = Math.floor((now - started) * current.fps / 1000) % current.frames.length;
    if (i !== frameIdx) { frameIdx = i; draw(current.frames[i]); }
  }
  requestAnimationFrame(tick);
}
select("Timer pie, word clock (5 min time-lapse)");
requestAnimationFrame(tick);
</script>
'''

with open(sys.argv[1], encoding="utf-8") as f:
    scenes = json.load(f)
with open(sys.argv[2], "w", encoding="utf-8") as f:
    f.write(PAGE.replace("__SCENES__", json.dumps(scenes, ensure_ascii=False, separators=(",", ":"))))
