/* The Digital World behind the page, and the device that tells the story.
 *
 * World: an endless data grid and rising particles, retinted per chapter ("dw3:scene" from app.js),
 * pulsed by WEAK! hits and shaken by Blast.
 *
 * Device: one 3D object that changes with the years as you scroll. A CRT TV with an old console
 * (2002), a handheld, a phone, a flat TV with a modern console, a PC. Each change happens while the
 * next chapter scrolls in: the device spins, its screen turns to static, and the new shape is there
 * when it faces you again. Its screen shows the game and the mechanic of the chapter on screen, and
 * repeats what the page's toys do (combat pops, hunt chain, Blast gauge).
 * Where it is drawn: wide screens use the fixed #devDock on the right; narrow screens draw it into
 * the .devslot of whichever chapter is on screen. Both are empty boxes the canvas shows through.
 *
 * If WebGL or the CDN fails, the CSS backdrop and callouts in index.html simply stay. */
import * as THREE from 'https://cdn.jsdelivr.net/npm/three@0.170.0/build/three.module.min.js';

const canvas = document.getElementById('scene');
const reduced = matchMedia('(prefers-reduced-motion: reduce)').matches;
const mobile = matchMedia('(max-width: 900px)').matches;
let STR = {};
try { STR = JSON.parse(document.getElementById('i18n-js').textContent); } catch {}
const T = (k) => STR[k] ?? k;

let renderer;
try {
  renderer = new THREE.WebGLRenderer({ canvas, antialias: !mobile, alpha: false, powerPreference: 'high-performance' });
} catch {
  throw new Error('WebGL unavailable; keeping the CSS backdrop');
}
renderer.setPixelRatio(Math.min(devicePixelRatio, mobile ? 1.5 : 1.75));
renderer.setClearColor(0x0d0f16, 1);
renderer.autoClear = false; // two passes: the world, then the device inside its box

const clamp = (v, a = 0, b = 1) => Math.min(b, Math.max(a, v));
const smooth = (p) => p * p * (3 - 2 * p);
const lerp = (a, b, k) => a + (b - a) * k;

/* =====================================================================================
 * World
 * ===================================================================================== */
const scene = new THREE.Scene();
scene.fog = new THREE.FogExp2(0x0d0f16, 0.055);
const camera = new THREE.PerspectiveCamera(60, 1, 0.1, 120);

/* Palettes per chapter: grid line colour, fog/clear colour, particle colour, grid speed. */
const PALETTES = {
  hero:     { grid: 0x6b5cff, fog: 0x0d0f16, dust: 0xb7afff, speed: 1.0, accent: 0x9085e9 },
  grind:    { grid: 0x55576a, fog: 0x0c0c10, dust: 0x8a8c99, speed: 0.25, accent: 0x8c8e9c },
  fight:    { grid: 0xef6a3a, fog: 0x140b0e, dust: 0xffb08a, speed: 1.6, accent: 0xef6a3a },
  journey:  { grid: 0x1aa874, fog: 0x08130f, dust: 0x9cf0c8, speed: 1.2, accent: 0x1aa874 },
  hunt:     { grid: 0xf0a93a, fog: 0x130e06, dust: 0xffd88a, speed: 1.1, accent: 0xf0a93a },
  velvet:   { grid: 0x3d6bff, fog: 0x050a2a, dust: 0x8fb0ff, speed: 0.5, accent: 0x5b84ff },
  story:    { grid: 0x9085e9, fog: 0x0e0c1c, dust: 0xd8d2ff, speed: 0.8, accent: 0xb7afff },
  blast:    { grid: 0xff2a4d, fog: 0x14040a, dust: 0xff8899, speed: 2.0, accent: 0xff3b5c },
  anywhere: { grid: 0x33c3d6, fog: 0x061316, dust: 0xa6f2fb, speed: 1.0, accent: 0x33c3d6 },
};
const cur = { grid: new THREE.Color(0x6b5cff), fog: new THREE.Color(0x0d0f16), dust: new THREE.Color(0xb7afff), accent: new THREE.Color(0x9085e9), speed: 1 };
let target = PALETTES.hero;

const gridMat = new THREE.ShaderMaterial({
  transparent: true,
  depthWrite: false,
  uniforms: { uColor: { value: cur.grid }, uPulse: { value: 0 }, uScroll: { value: 0 } },
  vertexShader: /* glsl */`
    varying vec3 vW;
    void main() { vec4 w = modelMatrix * vec4(position, 1.0); vW = w.xyz; gl_Position = projectionMatrix * viewMatrix * w; }
  `,
  fragmentShader: /* glsl */`
    uniform float uPulse, uScroll; uniform vec3 uColor; varying vec3 vW;
    float grid(vec2 p, float s) { vec2 g = abs(fract(p / s - 0.5) - 0.5) / fwidth(p / s); return 1.0 - min(min(g.x, g.y), 1.0); }
    void main() {
      vec2 p = vW.xz; p.y += uScroll;
      float l = grid(p, 1.0) * 0.55 + grid(p, 4.0) * 0.45;
      float d = length(vW.xz - vec2(0.0, -2.0));
      float fade = exp(-d * 0.065);
      // a ring of light that expands from the centre on WEAK!/Blast
      float ring = smoothstep(1.2, 0.0, abs(d - uPulse * 26.0)) * (1.0 - uPulse);
      vec3 c = uColor * (l * (0.9 + ring * 3.0) + ring * 0.35);
      gl_FragColor = vec4(c, (l * 0.85 + ring * 0.6) * fade);
    }
  `,
});
const floor = new THREE.Mesh(new THREE.PlaneGeometry(160, 160, 1, 1), gridMat);
floor.rotation.x = -Math.PI / 2;
floor.position.y = -1.6;
scene.add(floor);
// A mirrored ceiling grid, faint, gives the "inside a server" feeling.
const ceilMat = gridMat.clone();
ceilMat.uniforms = THREE.UniformsUtils.clone(gridMat.uniforms);
const ceil = new THREE.Mesh(floor.geometry, ceilMat);
ceil.rotation.x = Math.PI / 2;
ceil.position.y = 7.5;
scene.add(ceil);

function glowTexture() {
  const c = document.createElement('canvas');
  c.width = c.height = 128;
  const g = c.getContext('2d');
  const r = g.createRadialGradient(64, 64, 0, 64, 64, 64);
  r.addColorStop(0, 'rgba(255,255,255,1)');
  r.addColorStop(0.25, 'rgba(255,255,255,.45)');
  r.addColorStop(1, 'rgba(255,255,255,0)');
  g.fillStyle = r;
  g.fillRect(0, 0, 128, 128);
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  return t;
}
const glow = glowTexture();

const N = mobile ? 500 : 1400;
const pos = new Float32Array(N * 3), spd = new Float32Array(N);
for (let i = 0; i < N; i++) {
  pos[i * 3] = (Math.random() - 0.5) * 40;
  pos[i * 3 + 1] = Math.random() * 10 - 1.6;
  pos[i * 3 + 2] = -Math.random() * 40 + 6;
  spd[i] = 0.2 + Math.random() * 0.8;
}
const pGeo = new THREE.BufferGeometry();
pGeo.setAttribute('position', new THREE.BufferAttribute(pos, 3));
const pMat = new THREE.PointsMaterial({ size: mobile ? 0.09 : 0.07, map: glow, color: cur.dust, transparent: true, opacity: 0.85, depthWrite: false, blending: THREE.AdditiveBlending, sizeAttenuation: true });
scene.add(new THREE.Points(pGeo, pMat));

/* =====================================================================================
 * Device: geometry
 * ===================================================================================== */
const dev = new THREE.Scene();
const devCam = new THREE.PerspectiveCamera(26, 1, 0.1, 200);
dev.add(new THREE.HemisphereLight(0xd6d2ff, 0x16131f, 1.3));
const keyLight = new THREE.DirectionalLight(0xffffff, 2.4);
keyLight.position.set(-4, 6, 8);
dev.add(keyLight);
const rimLight = new THREE.DirectionalLight(0x9085e9, 4);
rimLight.position.set(6, 3, -6);
dev.add(rimLight);
const screenLight = new THREE.PointLight(0x9085e9, 8, 14, 2); // the screen's glow on keys and knobs
screenLight.position.set(0, -0.4, 2.6);
dev.add(screenLight);

const device = new THREE.Group(); // rotates around the visual centre
dev.add(device);
const rig = new THREE.Group(); // offset so each form's bounding box is centred
device.add(rig);

const std = (color, o = {}) => new THREE.MeshStandardMaterial({ color, roughness: 0.55, metalness: 0.12, ...o });
const bodyMat = std(0x2c2a31);
const darkMat = std(0x0b0b10, { roughness: 0.35 });
const metalMat = std(0x9a9cab, { metalness: 0.85, roughness: 0.28 });
const greyMat = std(0xb9b6bd, { roughness: 0.7 });
const whiteMat = std(0xe9e9f0, { roughness: 0.4 });
const lit = (color) => new THREE.MeshBasicMaterial({ color });
const add = (parent, geo, mat, x = 0, y = 0, z = 0, rot) => {
  const m = new THREE.Mesh(geo, mat);
  m.position.set(x, y, z);
  if (rot) m.rotation.set(rot[0] || 0, rot[1] || 0, rot[2] || 0);
  parent.add(m);
  return m;
};
const disc = (r, h, seg = 24) => new THREE.CylinderGeometry(r, r, h, seg); // rotate x by PI/2 to face the camera
const FACE = [Math.PI / 2, 0, 0];

function rrShape(w, h, r) {
  r = Math.max(0.001, Math.min(r, w / 2 - 0.001, h / 2 - 0.001));
  const s = new THREE.Shape(), x = -w / 2, y = -h / 2;
  s.moveTo(x + r, y);
  s.lineTo(x + w - r, y); s.quadraticCurveTo(x + w, y, x + w, y + r);
  s.lineTo(x + w, y + h - r); s.quadraticCurveTo(x + w, y + h, x + w - r, y + h);
  s.lineTo(x + r, y + h); s.quadraticCurveTo(x, y + h, x, y + h - r);
  s.lineTo(x, y + r); s.quadraticCurveTo(x, y, x + r, y);
  return s;
}

/* The five forms. body: [w, h, depth, corner radius]; scr: [w, h, x, y] of the screen;
 * fit: [w, h, cx, cy], the box the camera frames; crt: tube look (1) or flat panel (0). */
const FORMS = [
  { body: [4.4, 3.3, 2.6, 0.38], scr: [3.0, 2.25, -0.45, 0.05], color: 0x2c2a31, crt: 1, fit: [5.4, 6.0, 0, 0.7] },
  { body: [5.9, 2.75, 0.42, 1.1], scr: [3.0, 2.25, 0, 0.05], color: 0x43337a, crt: 0, fit: [6.6, 3.4, 0, 0.06] },
  { body: [4.9, 2.62, 0.2, 0.42], scr: [4.4, 2.36, 0.05, 0], color: 0x15171f, crt: 0, fit: [5.6, 3.1, 0, 0] },
  { body: [6.3, 3.65, 0.16, 0.08], scr: [6.05, 3.4, 0, 0], color: 0x0a0b0f, crt: 0, fit: [8.4, 4.8, 0.65, -0.2] },
  { body: [5.9, 3.5, 0.24, 0.12], scr: [5.55, 3.12, 0, 0.06], color: 0x1c2030, crt: 0, fit: [8.8, 5.6, -0.95, -0.45] },
];
// The chapter whose arrival turns the device into form i+1.
const MORPH_AT = ['fight', 'hunt', 'story', 'anywhere'];
// In the download section the platform tabs pick the form instead of the scroll.
const PLATFORM_FORM = { 't-pc': 4, 't-and': 2, 't-hh': 1 };
let platformForm = null;

const body = add(rig, new THREE.BufferGeometry(), bodyMat);
const bezel = add(rig, new THREE.BufferGeometry(), darkMat, 0, 0, 0.004);
let lastDims = '';
function rebuild(w, h, d, r, sw, sh, sx, sy) {
  const k = [w, h, d, r, sw, sh].map((v) => v.toFixed(3)).join();
  if (k === lastDims) return;
  lastDims = k;
  body.geometry.dispose();
  const depth = Math.max(0.02, d - 0.08);
  const g = new THREE.ExtrudeGeometry(rrShape(w, h, r), { depth, bevelEnabled: true, bevelThickness: 0.04, bevelSize: 0.04, bevelSegments: 2, curveSegments: 10 });
  g.translate(0, 0, -(depth + 0.04)); // front face at z = 0
  body.geometry = g;
  bezel.geometry.dispose();
  bezel.geometry = new THREE.ShapeGeometry(rrShape(sw + 0.22, sh + 0.22, 0.1));
  bezel.position.set(sx, sy, 0.004);
}

/* ---- screen: a 16:9 canvas; 4:3 screens show its centre, CRTs bulge and scan ---- */
const SW = 640, SH = 360;
const scrCanvas = document.createElement('canvas');
scrCanvas.width = SW; scrCanvas.height = SH;
const g = scrCanvas.getContext('2d');
const scrTex = new THREE.CanvasTexture(scrCanvas);
scrTex.minFilter = THREE.LinearFilter;
scrTex.generateMipmaps = false;
const scrMat = new THREE.ShaderMaterial({
  uniforms: { map: { value: scrTex }, uCrt: { value: 1 }, uTime: { value: 0 }, uStatic: { value: 0 }, uFlash: { value: 0 }, uAspect: { value: 4 / 3 } },
  vertexShader: /* glsl */`
    uniform float uCrt; varying vec2 vUv;
    void main() {
      vUv = uv; vec3 p = position; vec2 c = uv * 2.0 - 1.0;
      p.z += uCrt * 0.09 * (1.0 - 0.5 * dot(c, c)); // tube glass bulge
      gl_Position = projectionMatrix * modelViewMatrix * vec4(p, 1.0);
    }
  `,
  fragmentShader: /* glsl */`
    uniform sampler2D map; uniform float uCrt, uTime, uStatic, uFlash, uAspect; varying vec2 vUv;
    float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
    void main() {
      vec2 c = vUv * 2.0 - 1.0; c *= 1.0 + 0.07 * uCrt * dot(c, c);
      vec2 uv = c * 0.5 + 0.5;
      if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) { gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }
      vec2 t = vec2(0.5 + (uv.x - 0.5) * uAspect / (16.0 / 9.0), uv.y);
      float ca = 0.0022 * uCrt;
      vec3 col = vec3(texture2D(map, t + vec2(ca, 0.0)).r, texture2D(map, t).g, texture2D(map, t - vec2(ca, 0.0)).b);
      float scan = 0.5 + 0.5 * sin(uv.y * 240.0 * 3.14159);
      col *= mix(1.0, 0.6 + 0.4 * scan, 0.55 * uCrt);
      float n = hash(floor(uv * vec2(260.0, 150.0)) + floor(uTime * 30.0));
      col = mix(col, vec3(n * 0.85), uStatic);
      vec2 d = uv - 0.5; col *= 1.0 - dot(d, d) * (1.2 * uCrt + 0.2);
      col += uFlash * vec3(1.0, 0.18, 0.3);
      gl_FragColor = vec4(col * 1.06, 1.0);
    }
  `,
});
const screen = add(rig, new THREE.PlaneGeometry(1, 1, 24, 18), scrMat, 0, 0, 0.012);

/* ---- parts per form ---- */
const parts = FORMS.map(() => { const p = new THREE.Group(); rig.add(p); return p; });

// 0 · CRT TV and the grey console under it
{
  const p = parts[0];
  for (const y of [0.78, 0.12]) {
    add(p, disc(0.24, 0.16, 28), darkMat, 1.72, y, 0.08, FACE);
    add(p, new THREE.BoxGeometry(0.045, 0.17, 0.03), metalMat, 1.72, y + 0.1, 0.17);
  }
  for (let i = 0; i < 5; i++) add(p, new THREE.BoxGeometry(0.72, 0.05, 0.03), darkMat, 1.72, -0.42 - i * 0.16, 0.02);
  add(p, new THREE.SphereGeometry(0.05, 12, 8), lit(0xff3355), 1.98, -1.32, 0.03);
  const tubeGeo = new THREE.CylinderGeometry(1.0, 2.05, 1.8, 4, 1);
  tubeGeo.rotateY(Math.PI / 4);
  add(p, tubeGeo, bodyMat, 0, 0, -3.45, [-Math.PI / 2, 0, 0]);
  for (const x of [-1.4, 1.4]) add(p, new THREE.BoxGeometry(0.7, 0.14, 1.6), darkMat, x, -1.72, -1.3);
  const ant = new THREE.Group();
  ant.position.set(0, 1.65, -1.1);
  add(ant, new THREE.SphereGeometry(0.3, 20, 10, 0, Math.PI * 2, 0, Math.PI / 2), darkMat);
  for (const s of [-1, 1]) {
    const rod = new THREE.CylinderGeometry(0.022, 0.032, 2.0, 8);
    rod.translate(0, 1.0, 0);
    add(ant, rod, metalMat, 0, 0.12, 0, [0, 0, s * 0.5]);
    add(ant, new THREE.SphereGeometry(0.055, 10, 8), metalMat, -Math.sin(s * 0.5) * 2.0, 0.12 + Math.cos(s * 0.5) * 2.0, 0);
  }
  p.add(ant);
  // the console of 2002, plugged in under the set
  const ps = new THREE.Group();
  ps.position.set(-0.55, -1.98, 1.0);
  add(ps, new THREE.BoxGeometry(2.0, 0.3, 1.35), greyMat);
  add(ps, disc(0.42, 0.04, 32), std(0xa9a6ae), -0.35, 0.16, -0.05);
  add(ps, disc(0.07, 0.05, 16), darkMat, 0.62, 0.16, 0.3);
  add(ps, disc(0.07, 0.05, 16), darkMat, 0.62, 0.16, -0.05);
  add(ps, new THREE.BoxGeometry(0.14, 0.03, 0.03), lit(0x36c98a), 0.75, 0.05, 0.68);
  p.add(ps);
}
// 1 · handheld
{
  const p = parts[1];
  add(p, disc(0.52, 0.02, 32), darkMat, -2.35, 0.18, 0.012, FACE);
  add(p, new THREE.BoxGeometry(0.68, 0.22, 0.12), std(0x171126), -2.35, 0.18, 0.07);
  add(p, new THREE.BoxGeometry(0.22, 0.68, 0.12), std(0x171126), -2.35, 0.18, 0.07);
  [[0x4aa3ff, 0, 0.32], [0xef6a3a, 0.32, 0], [0x36c98a, 0, -0.32], [0xf3c843, -0.32, 0]].forEach(([c, dx, dy]) => {
    add(p, disc(0.14, 0.12, 20), std(c, { emissive: c, emissiveIntensity: 0.35 }), 2.35 + dx, 0.18 + dy, 0.07, FACE);
  });
  for (const x of [2.15, 2.6]) add(p, new THREE.BoxGeometry(0.3, 0.08, 0.06), std(0x171126), x, -0.82, 0.04);
  for (let i = 0; i < 9; i++) add(p, disc(0.035, 0.02, 10), darkMat, -2.5 + (i % 3) * 0.15, -0.72 - Math.floor(i / 3) * 0.15, 0.012, FACE);
  for (const x of [-2.2, 2.2]) add(p, new THREE.BoxGeometry(1.1, 0.16, 0.3), std(0x2b2147), x, 1.42, -0.21);
  // the XF35H's LED bar, in the five element colours
  [0xef6a3a, 0x4aa3ff, 0x36c98a, 0xf3c843, 0xa77bff].forEach((c, i) => add(p, new THREE.BoxGeometry(0.22, 0.045, 0.02), lit(c), -0.5 + i * 0.25, -1.24, 0.012));
}
// 2 · phone (landscape)
{
  const p = parts[2];
  add(p, disc(0.06, 0.02, 16), std(0x05060a, { roughness: 0.1 }), -2.27, 0, 0.012, FACE);
  for (const x of [0.9, 1.55]) add(p, new THREE.BoxGeometry(0.5, 0.06, 0.1), metalMat, x, 1.33, -0.1);
}
// 3 · flat TV on a pedestal, a modern console beside it
{
  const p = parts[3];
  add(p, new THREE.BoxGeometry(0.6, 0.45, 0.16), darkMat, 0, -2.02, -0.12);
  add(p, new THREE.BoxGeometry(2.6, 0.07, 1.1), darkMat, 0, -2.23, 0);
  const box = new THREE.Group();
  box.position.set(4.1, -1.0, -0.4);
  add(box, new THREE.BoxGeometry(0.42, 2.5, 2.1), whiteMat, -0.26, 0, 0);
  add(box, new THREE.BoxGeometry(0.12, 2.5, 2.0), darkMat, 0, 0, 0);
  add(box, new THREE.BoxGeometry(0.42, 2.5, 2.1), whiteMat, 0.26, 0, 0);
  add(box, new THREE.BoxGeometry(0.04, 2.3, 0.03), lit(0x4a7cff), -0.06, 0, 1.06);
  p.add(box);
}
// 4 · PC: monitor on a stand, keyboard, mouse and a tower with lit fans
{
  const p = parts[4];
  add(p, new THREE.BoxGeometry(0.42, 1.1, 0.25), darkMat, 0, -2.2, -0.35);
  add(p, new THREE.BoxGeometry(2.4, 0.1, 1.3), darkMat, 0, -2.76, -0.1);
  add(p, new THREE.BoxGeometry(4.8, 0.14, 1.5), std(0x15171f), 0, -2.75, 1.95, [0.08, 0, 0]);
  for (let r = 0; r < 4; r++) add(p, new THREE.BoxGeometry(4.4, 0.03, 0.22), std(0x2a2f44), 0, -2.66 + r * 0.03, 1.45 + r * 0.3, [0.08, 0, 0]);
  add(p, new THREE.BoxGeometry(4.8, 0.03, 0.03), lit(0x9085e9), 0, -2.83, 2.7);
  add(p, new THREE.SphereGeometry(0.22, 16, 10), std(0x15171f), 3.15, -2.74, 1.95, [0, 0, 0]).scale.set(1, 0.45, 1.5);
  const tower = new THREE.Group();
  tower.position.set(-4.3, -1.05, -0.9);
  add(tower, new THREE.BoxGeometry(1.3, 3.6, 3.0), std(0x161924));
  for (const y of [0.75, -0.45]) {
    add(tower, new THREE.TorusGeometry(0.42, 0.05, 8, 32), lit(0xa77bff), 0, y, 1.52);
    add(tower, disc(0.36, 0.02, 24), std(0x2a2f44), 0, y, 1.51, FACE);
  }
  add(tower, new THREE.BoxGeometry(0.06, 0.06, 0.03), lit(0x33c3d6), 0.45, 1.6, 1.52);
  p.add(tower);
}

// Five element orbs circle the device on the first screen only.
const ELEMENTS = [0xef6a3a, 0x4aa3ff, 0x36c98a, 0xf3c843, 0xa77bff];
const orbs = ELEMENTS.map((col, i) => {
  const o = new THREE.Group();
  const m = new THREE.Mesh(new THREE.OctahedronGeometry(0.2, 0), new THREE.MeshBasicMaterial({ color: col, wireframe: true, transparent: true }));
  const s = new THREE.Sprite(new THREE.SpriteMaterial({ map: glow, color: col, transparent: true, opacity: 0.9, blending: THREE.AdditiveBlending, depthWrite: false }));
  s.scale.setScalar(1.1);
  o.add(m, s);
  o.userData = { a: (i / ELEMENTS.length) * Math.PI * 2, m, s };
  dev.add(o);
  return o;
});

/* =====================================================================================
 * Device: what its screen shows
 * ===================================================================================== */
const IMG = {};
const img = (k) => {
  if (!IMG[k]) { const i = new Image(); i.decoding = 'async'; i.src = { logo: '/assets/logo.png', rare: '/assets/shots/battle-weak.jpg' }[k] || `/assets/shots/${k}.png`; IMG[k] = i; }
  return IMG[k];
};
const VID = {};
let playing = null, wantVideo = null;
const vid = (k) => {
  if (!VID[k]) {
    const v = document.createElement('video');
    v.muted = true; v.loop = true; v.playsInline = true; v.preload = 'auto';
    v.setAttribute('muted', ''); v.setAttribute('playsinline', '');
    v.src = `/assets/shots/${k}.mp4`;
    VID[k] = v;
  }
  wantVideo = VID[k];
  return VID[k];
};
const ready = (s) => s && (s.tagName === 'VIDEO' ? s.readyState >= 2 : s.complete && s.naturalWidth > 0);

const FONT = '"Chakra Petch", "Segoe UI", sans-serif';
const MONO = '"JetBrains Mono", ui-monospace, monospace';
const BODY = 'Manrope, system-ui, sans-serif';
let wide = false; // the current screen is wider than 4:3, so the canvas sides are visible

function text(s, x, y, size, color, align = 'center', font = FONT, weight = 700) {
  g.font = `${weight} ${size}px ${font}`;
  g.fillStyle = color;
  g.textAlign = align;
  g.textBaseline = 'middle';
  g.fillText(s, x, y);
}
// A 4:3 picture in the centre; on wide screens its blurred, darkened self fills the sides.
function shot(src, alpha = 1) {
  if (!ready(src)) return false;
  g.save();
  g.globalAlpha = alpha;
  if (wide) {
    g.filter = 'blur(10px)';
    g.drawImage(src, -20, -80, SW + 40, (SW + 40) * 0.75);
    g.filter = 'none';
    g.fillStyle = 'rgba(5,6,12,.62)';
    g.fillRect(0, 0, SW, SH);
  }
  g.drawImage(src, 80, 0, 480, 360);
  g.restore();
  return true;
}
// The page's slanted callout: flies in big, settles, fades up.
function stamp(s, bg, x, y, age, size = 46, life = 1.6) {
  if (age < 0 || age > life) return;
  const sc = age < 0.16 ? 2.4 - 1.46 * (age / 0.16) : age < 0.24 ? 0.94 + 0.06 * ((age - 0.16) / 0.08) : 1;
  const out = age > life - 0.3 ? (life - age) / 0.3 : 1;
  g.save();
  g.globalAlpha = clamp(out);
  g.translate(x, y - (1 - out) * 18);
  g.rotate(-6 * Math.PI / 180);
  g.scale(sc, sc);
  g.font = `italic 700 ${size}px ${FONT}`;
  const w = g.measureText(s).width + size * 0.8, h = size * 1.15;
  g.fillStyle = bg;
  g.fillRect(-w / 2, -h / 2, w, h);
  g.fillStyle = '#0d0f16';
  g.textAlign = 'center';
  g.textBaseline = 'middle';
  g.fillText(s, 0, 3);
  g.restore();
}
function wrap(s, x, y, maxW, lh) {
  const words = s.split(' ');
  let line = '';
  for (const w of words) {
    const test = line ? line + ' ' + w : w;
    if (g.measureText(test).width > maxW && line) { g.fillText(line, x, y); line = w; y += lh; } else line = test;
  }
  g.fillText(line, x, y);
}
const blink = (t, hz = 1.2) => Math.floor(t * hz * 2) % 2 === 0;

const POP_BG = { weak: '#ef6a3a', more: '#1aa874', resist: '#8a8fa6', over: '#ffb03a', baton: '#b7afff', plain: '#e6e4f2' };
const live = { pops: [], chain: 0, rare: false, rareAt: 0, gauge: 0, blastAt: -9 };

const SCREENS = {
  hero(t) {
    const r = g.createRadialGradient(SW / 2, SH / 2, 20, SW / 2, SH / 2, 360);
    r.addColorStop(0, '#2a2260'); r.addColorStop(1, '#07080f');
    g.fillStyle = r; g.fillRect(0, 0, SW, SH);
    const l = img('logo');
    if (ready(l)) g.drawImage(l, 120, 70, 400, 113);
    if (blink(t, 0.9)) text(T('dev.pressStart'), SW / 2, 300, 24, '#f0efec', 'center', MONO);
    const ph = t % 4.6;
    stamp(T('pop.weak'), '#ef6a3a', 250, 225, ph - 0.5, 40, 3.3);
    stamp(T('pop.more'), '#1aa874', 400, 255, ph - 1.0, 28, 2.8);
  },
  grind(t, el) {
    g.fillStyle = '#16171c'; g.fillRect(0, 0, SW, SH);
    for (let y = 0; y < SH; y += 6) { g.fillStyle = `rgba(255,255,255,${0.015 + 0.015 * Math.sin(y * 0.3 + t)})`; g.fillRect(0, y, SW, 3); }
    g.fillStyle = '#050a20'; g.strokeStyle = '#c7c7d3'; g.lineWidth = 4;
    g.fillRect(110, 110, 170, 176); g.strokeRect(110, 110, 170, 176);
    ['fight', 'tech', 'item', 'run'].forEach((k, i) => text(T('dev.menu.' + k), 150, 140 + i * 38, 22, i ? '#6e6f7d' : '#e2e2ea', 'left', MONO));
    if (blink(t, 1)) text('▶', 128, 140, 18, '#ffffff', 'left', MONO);
    const lv = Math.min(40, Math.floor(1 + 39 * (1 - Math.exp(-el / 7))));
    text(T('dev.lv'), 340, 160, 26, '#6c6e7c', 'left', FONT);
    text(String(lv), 395, 190, 104, '#8a8c99', 'left', FONT);
    text(T('dev.stuck'), 340, 268, 16, '#6c6e7c', 'left', MONO, 500);
    text('2002', 540, 40, 16, '#55576a', 'center', MONO);
  },
  fight(t) {
    if (!shot(vid('battle-weak'))) { g.fillStyle = '#140b0e'; g.fillRect(0, 0, SW, SH); }
  },
  journey(t, el) {
    if (!shot(vid('overlevel-rush'))) { g.fillStyle = '#08130f'; g.fillRect(0, 0, SW, SH); }
    g.fillStyle = 'rgba(6,10,14,.84)'; g.fillRect(96, 272, 448, 74);
    text(T('dev.tp'), 112, 290, 14, '#b9b8c6', 'left', MONO, 500);
    const p = smooth(clamp(el / 2.2));
    g.fillStyle = '#1aa874'; g.fillRect(112, 304, 340 * p, 12);
    g.fillStyle = '#5d6074'; g.fillRect(112, 324, 340 * 0.8 * smooth(clamp(el / 3.6)), 12);
    text(String(Math.round(613 * p)), 528, 310, 18, '#9cf0c8', 'right', FONT);
    text(String(Math.round(490 * smooth(clamp(el / 3.6)))), 528, 330, 18, '#8a8c99', 'right', FONT);
  },
  hunt(t) {
    if (live.rare) {
      g.filter = 'hue-rotate(120deg) saturate(1.3)';
      shot(img('rare'));
      g.filter = 'none';
      stamp(T('dev.rare'), '#f0a93a', SW / 2, 70, Math.min(t - live.rareAt, 1), 34, 99);
      return;
    }
    if (!shot(vid('battle-overfinish'), 0.55)) { g.fillStyle = '#130e06'; g.fillRect(0, 0, SW, SH); }
    g.fillStyle = 'rgba(10,8,4,.78)'; g.fillRect(96, 250, 448, 92);
    text(T('dev.chain'), 112, 268, 14, '#f0a93a', 'left', MONO);
    for (let i = 0; i < 10; i++) {
      const on = i < live.chain;
      g.fillStyle = on ? (i >= 5 ? '#ffb03a' : '#f0a93a') : 'rgba(255,255,255,.08)';
      g.fillRect(112 + i * 42, 286, 34, 26);
      if (i >= 5) text((i - 4) * 20 + '%', 129 + i * 42, 328, 11, '#7f8296', 'center', MONO, 500);
    }
  },
  velvet(t, el) {
    const ph = el % 12;
    g.fillStyle = '#050a2a'; g.fillRect(0, 0, SW, SH);
    if (ph < 7.5) shot(vid('velvet-way-in'));
    if (ph > 6.5) shot(img('velvet-room'), clamp(ph < 11 ? ph - 6.5 : 12 - ph));
    g.fillStyle = 'rgba(30,46,120,.85)'; g.fillRect(96, 16, 150, 28);
    text(T('dev.velvet'), 171, 31, 13, '#cfdcff', 'center', MONO);
  },
  story(t, el) {
    g.fillStyle = '#0e0c1c'; g.fillRect(0, 0, SW, SH);
    shot(img('velvet-room'), 0.45);
    const ph = el % 11;
    const gr = g.createLinearGradient(0, 214, 0, 340);
    gr.addColorStop(0, '#1e2e78'); gr.addColorStop(1, '#0a0e28');
    g.fillStyle = gr; g.fillRect(96, 214, 448, 128);
    g.strokeStyle = '#8fb0ff'; g.lineWidth = 2; g.strokeRect(97, 215, 446, 126);
    g.fillStyle = '#8fb0ff'; g.fillRect(110, 200, 92, 24);
    text(T('dev.igor'), 156, 212, 15, '#0a0e28', 'center', FONT);
    const line = T('dev.igorLine');
    const n = Math.min(line.length, Math.floor(ph * 34));
    g.font = `500 17px ${BODY}`; g.fillStyle = '#eef1ff'; g.textAlign = 'left'; g.textBaseline = 'top';
    wrap(line.slice(0, n), 116, 236, 412, 24);
    if (n >= line.length) {
      g.fillStyle = 'rgba(10,14,40,.92)'; g.fillRect(380, 118, 160, 76);
      g.strokeStyle = '#8fb0ff'; g.strokeRect(380, 118, 160, 76);
      text(T('dev.answer1'), 408, 140, 15, '#ffffff', 'left', BODY);
      text(T('dev.answer2'), 408, 172, 15, '#9aa6d8', 'left', BODY);
      if (blink(t, 1.4)) text('▶', 390, 140, 12, '#8fb0ff', 'left', MONO);
    }
  },
  blast(t) {
    if (!shot(vid('battle-baton-pass'))) { g.fillStyle = '#14040a'; g.fillRect(0, 0, SW, SH); }
    g.fillStyle = `rgba(255,30,70,${0.14 + 0.06 * Math.sin(t * 3)})`; g.fillRect(0, 0, SW, SH);
    g.fillStyle = 'rgba(14,4,8,.82)'; g.fillRect(96, 290, 448, 54);
    text(T('dev.gauge'), 112, 304, 13, '#ff8899', 'left', MONO);
    g.fillStyle = 'rgba(255,255,255,.08)'; g.fillRect(112, 318, 416, 14);
    g.fillStyle = live.gauge >= 100 && blink(t, 3) ? '#ff8899' : '#ff3b5c';
    g.fillRect(112, 318, 416 * live.gauge / 100, 14);
    stamp(T('dev.blast'), '#ff3b5c', SW / 2, 150, t - live.blastAt, 64, 1.8);
  },
  anywhere(t, el) {
    const keys = ['battle-inspect', 'guide-quest', 'battle-hud'];
    const ph = el / 3.4, i = Math.floor(ph) % 3, f = ph % 1;
    g.fillStyle = '#061316'; g.fillRect(0, 0, SW, SH);
    shot(img(keys[i]));
    if (f > 0.85) shot(img(keys[(i + 1) % 3]), (f - 0.85) / 0.15);
    const syncing = (el % 3.4) < 1.7;
    g.fillStyle = 'rgba(4,16,20,.85)'; g.fillRect(SW - 236, 14, 222, 30);
    g.save(); g.translate(SW - 218, 29);
    if (syncing) { g.rotate(t * 5); g.strokeStyle = '#33c3d6'; g.lineWidth = 2.5; g.beginPath(); g.arc(0, 0, 7, 0, Math.PI * 1.5); g.stroke(); }
    else text('✓', 0, 1, 15, '#33c3d6', 'center', MONO);
    g.restore();
    text(syncing ? T('dev.sync') : T('dev.synced'), SW - 200, 30, 12, '#a6f2fb', 'left', MONO);
    g.fillStyle = 'rgba(4,16,20,.8)'; g.fillRect(SW / 2 - 130, SH - 36, 260, 24);
    text(T('dev.noEmu'), SW / 2, SH - 24, 12, '#a6f2fb', 'center', MONO);
  },
  get(t, el) {
    g.fillStyle = '#06070c'; g.fillRect(0, 0, SW, SH);
    g.strokeStyle = 'rgba(144,133,233,.12)'; g.lineWidth = 1;
    for (let x = 0; x < SW; x += 32) { g.beginPath(); g.moveTo(x, 0); g.lineTo(x, SH); g.stroke(); }
    for (let y = 0; y < SH; y += 32) { g.beginPath(); g.moveTo(0, y); g.lineTo(SW, y); g.stroke(); }
    const l = img('logo');
    if (ready(l)) g.drawImage(l, 220, 30, 200, 57);
    text(T('dev.continue'), SW / 2, 150, 44, '#f0efec', 'center', FONT);
    text(String(9 - Math.floor(el % 10)), SW / 2, 228, 84, '#b7afff', 'center', FONT);
    if (blink(t, 0.9)) text(T('dev.pressStart'), SW / 2, 310, 20, '#f0efec', 'center', MONO);
  },
};
SCREENS.faq = SCREENS.get;

let shownCh = '', since = 0;
function drawScreen(ch, t) {
  if (ch !== shownCh) { shownCh = ch; since = t; }
  wantVideo = null;
  g.save();
  (SCREENS[ch] || SCREENS.hero)(t, t - since);
  g.restore();
  // combat pops from the demo, stamped on whatever is on screen
  live.pops = live.pops.filter((p) => t - p.at < 1.3);
  for (const p of live.pops) stamp(p.text, POP_BG[p.cls] || '#e6e4f2', p.x, p.y, t - p.at, p.cls === 'more' ? 30 : 40, 1.2);
  if (wantVideo !== playing) {
    playing?.pause();
    playing = wantVideo;
    playing?.play().catch(() => {});
  }
}

/* =====================================================================================
 * Layout, input, page events
 * ===================================================================================== */
let W = 0, H = 0;
const dockEl = document.getElementById('devDock');
const slots = [...document.querySelectorAll('.devslot')];
const footer = document.querySelector('footer');
const secs = Object.fromEntries(MORPH_AT.map((id) => [id, document.getElementById(id)]));
let useDock = false;
function resize() {
  W = innerWidth; H = innerHeight;
  renderer.setSize(W, H, false);
  camera.aspect = W / H;
  camera.updateProjectionMatrix();
  useDock = !!dockEl && getComputedStyle(dockEl).display !== 'none';
}
addEventListener('resize', resize);
resize();

// Where the device goes this frame: the dock, or the chapter slot nearest the middle of the screen.
let chapter = 'hero';
function anchor() {
  if (useDock) return { r: dockEl.getBoundingClientRect(), ch: chapter };
  let best = null, bd = Infinity;
  for (const el of slots) {
    const r = el.getBoundingClientRect();
    if (r.bottom < 0 || r.top > H || r.height < 10) continue;
    const d = Math.abs((r.top + r.bottom) / 2 - H / 2);
    if (d < bd) { bd = d; best = { r, ch: el.dataset.ch }; }
  }
  return best;
}

const pointer = { x: 0, y: 0, tx: 0, ty: 0 };
addEventListener('pointermove', (e) => { pointer.tx = (e.clientX / W) * 2 - 1; pointer.ty = (e.clientY / H) * 2 - 1; }, { passive: true });
// Phones: a gentle tilt parallax when the browser allows it without a permission prompt.
addEventListener('deviceorientation', (e) => {
  if (e.gamma == null) return;
  pointer.tx = clamp(e.gamma / 30, -1, 1);
  pointer.ty = clamp((e.beta - 45) / 30, -1, 1);
}, { passive: true });

let pulse = 1, shake = 0, t = 0;
addEventListener('dw3:scene', (e) => { target = PALETTES[e.detail] || PALETTES.hero; });
addEventListener('dw3:chapter', (e) => { chapter = e.detail; });
addEventListener('dw3:weak', () => { pulse = 0; });
addEventListener('dw3:blast', () => { pulse = 0; shake = reduced ? 0 : 1; live.blastAt = t; });
addEventListener('dw3:pop', (e) => { live.pops.push({ ...e.detail, at: t, x: SW / 2 + (Math.random() - 0.5) * 120, y: 120 + Math.random() * 90 }); });
addEventListener('dw3:chain', (e) => { live.chain = e.detail; });
addEventListener('dw3:rare', (e) => { live.rare = !!e.detail; live.rareAt = t; });
addEventListener('dw3:gauge', (e) => { live.gauge = e.detail; });
addEventListener('dw3:platform', (e) => { platformForm = PLATFORM_FORM[e.detail] ?? null; });

/* =====================================================================================
 * Loop
 * ===================================================================================== */
const clock = new THREE.Clock();
let scrollAcc = 0, running = true;
const tmp = new THREE.Color(), tmp2 = new THREE.Color();

// Scroll -> form: each MORPH_AT chapter turns the device into the next form while its top travels
// from the bottom of the screen to near the top. s runs 0..4; its fraction is the change in progress.
function scrollForm() {
  let s = 0;
  MORPH_AT.forEach((id) => {
    const el = secs[id];
    if (el) s += smooth(clamp((H * 0.95 - el.getBoundingClientRect().top) / (H * 0.8)));
  });
  return s;
}
// The form shown follows the scroll; in the download section a picked platform tab overrides it.
// Either way it travels at a capped speed, so a tab click plays the same spin-and-static change
// that scrolling does. Static peaks halfway between two forms.
let sShown = 0, sInit = false;
function morph(ch, dt) {
  const s = (ch === 'get' || ch === 'faq') && platformForm != null ? platformForm : scrollForm();
  if (!sInit) { sShown = s; sInit = true; }
  const step = 2.2 * dt;
  sShown += clamp(s - sShown, -step, step);
  if (Math.abs(s - sShown) < 1e-3) sShown = s;
  const f = sShown - Math.floor(sShown);
  return { s: sShown, stat: f > 0.001 && f < 0.999 ? Math.sin(f * Math.PI) : 0 };
}

function placeDevice(s, dt) {
  const i = Math.min(Math.floor(s), FORMS.length - 2);
  const f = clamp(s - i);
  const A = FORMS[i], B = FORMS[i + 1];
  // shapes change while the device is turned away (the middle of the spin)
  const k = smooth(clamp((f - 0.3) / 0.4));
  const L = (a, b) => lerp(a, b, k);
  const [bw, bh, bd, br] = A.body.map((v, j) => L(v, B.body[j]));
  const [sw, sh, sx, sy] = A.scr.map((v, j) => L(v, B.scr[j]));
  rebuild(bw, bh, bd, br, sw, sh, sx, sy);
  screen.scale.set(sw, sh, 1);
  screen.position.set(sx, sy, 0.012);
  bodyMat.color.set(A.color).lerp(tmp.set(B.color), k);
  scrMat.uniforms.uCrt.value = L(A.crt, B.crt);
  scrMat.uniforms.uAspect.value = sw / sh;
  wide = sw / sh > 1.5;
  parts.forEach((p, j) => {
    const w = clamp(1 - Math.abs(s - j) * 2.2);
    p.visible = w > 0.01;
    p.scale.setScalar(Math.max(0.001, smooth(w)));
  });
  const [fw, fh, cx, cy] = A.fit.map((v, j) => L(v, B.fit[j]));
  rig.position.set(-cx, -cy, 0);
  return { fw, fh };
}

function frame(dt) {
  t += dt;
  const ease = 1 - Math.exp(-dt * 2.2); // frame-rate independent easing toward the chapter palette
  cur.grid.lerp(tmp.set(target.grid), ease);
  cur.fog.lerp(tmp.set(target.fog), ease);
  cur.dust.lerp(tmp.set(target.dust), ease);
  cur.accent.lerp(tmp.set(target.accent), ease);
  cur.speed += (target.speed - cur.speed) * ease;

  scene.fog.color.copy(cur.fog);
  renderer.setClearColor(cur.fog);
  gridMat.uniforms.uColor.value.copy(cur.grid);
  ceilMat.uniforms.uColor.value.copy(cur.grid).multiplyScalar(0.35);
  pMat.color.copy(cur.dust);

  scrollAcc += dt * cur.speed * 1.4;
  gridMat.uniforms.uScroll.value = scrollAcc;
  ceilMat.uniforms.uScroll.value = -scrollAcc;
  pulse = Math.min(1, pulse + dt * 0.9);
  gridMat.uniforms.uPulse.value = pulse;

  const a = pGeo.attributes.position.array;
  for (let i = 0; i < N; i++) {
    a[i * 3 + 1] += spd[i] * dt * (0.35 + cur.speed * 0.3);
    if (a[i * 3 + 1] > 8) a[i * 3 + 1] = -1.6;
  }
  pGeo.attributes.position.needsUpdate = true;

  // world camera: slow breathing drift + pointer parallax + Blast shake
  pointer.x += (pointer.tx - pointer.x) * 0.05;
  pointer.y += (pointer.ty - pointer.y) * 0.05;
  const px = reduced ? 0 : pointer.x, py = reduced ? 0 : pointer.y;
  shake = Math.max(0, shake - dt * 1.6);
  const sh = shake * shake * 0.25;
  camera.position.set(px * 0.6 + (reduced ? 0 : Math.sin(t * 0.2) * 0.15) + (Math.random() - 0.5) * sh, 1.1 - py * 0.35 + (Math.random() - 0.5) * sh, 6);
  camera.lookAt(px * 0.2, 0.7, -4);

  renderer.setScissorTest(false);
  renderer.setViewport(0, 0, W, H);
  renderer.clear();
  renderer.render(scene, camera);

  // ---- the device ----
  const an = anchor();
  const ft = footer ? footer.getBoundingClientRect().top : Infinity;
  const power = 1 - clamp((H - ft) / (H * 0.5)); // switches off as the footer comes up
  if (!an || power <= 0.01 || an.r.width < 20) { playing?.pause(); playing = null; return; }

  const { s, stat } = morph(an.ch, dt);
  const { fw, fh } = placeDevice(s, dt);
  const r = an.r;
  const aspect = r.width / r.height;
  const tanH = Math.tan(THREE.MathUtils.degToRad(devCam.fov / 2));
  const dist = Math.max(fh / 2 / tanH, fw / 2 / (tanH * aspect)) * 1.04 + 0.9;
  devCam.aspect = aspect;
  devCam.position.set(0, 0, dist);
  devCam.lookAt(0, 0, 0);
  devCam.updateProjectionMatrix();

  // a full turn per change of form, plus a gentle idle sway; it faces the text on wide screens
  const base = useDock ? -0.32 : -0.14;
  device.rotation.set(
    0.05 + (reduced ? 0 : py * 0.1),
    base + (reduced ? 0 : s * Math.PI * 2 + px * 0.22 + Math.sin(t * 0.5) * 0.08),
    0
  );
  device.position.y = reduced ? 0 : Math.sin(t * 0.9) * 0.05;
  // CRT switch-off: squash to a line, then to a dot
  device.scale.set(Math.max(0.001, clamp(power * 3)), Math.max(0.001, clamp(power * 1.6 - 0.1)), 1);

  // the screen
  drawScreen(an.ch, t);
  scrTex.needsUpdate = true;
  scrMat.uniforms.uTime.value = t;
  scrMat.uniforms.uStatic.value = stat * 0.9;
  scrMat.uniforms.uFlash.value = Math.max(0, 0.8 - (t - live.blastAt) * 1.2);
  screenLight.color.copy(cur.accent);
  rimLight.color.copy(tmp2.copy(cur.accent).lerp(tmp.set(0xffffff), 0.2));

  // orbs: first screen only
  const heroVis = clamp(1 - scrollY / (H * 0.7));
  orbs.forEach((o, i) => {
    o.visible = heroVis > 0.01;
    if (!o.visible) return;
    const u = o.userData;
    const ang = u.a + t * 0.45;
    o.position.set(Math.cos(ang) * fw * 0.5, Math.sin(ang) * 0.5 + 0.2 + Math.sin(t * 1.3 + i) * 0.08, Math.sin(ang) * 2.6);
    u.m.rotation.set(t * 0.8, t * 1.5 + i, 0);
    u.m.material.opacity = heroVis;
    u.s.material.opacity = (0.75 + Math.sin(t * 3 + i * 1.7) * 0.2) * heroVis;
  });

  renderer.setViewport(r.left, H - r.bottom, r.width, r.height);
  renderer.setScissor(r.left, H - r.bottom, r.width, r.height);
  renderer.setScissorTest(true);
  renderer.clearDepth();
  renderer.render(dev, devCam);
  renderer.setScissorTest(false);
}

function loop() {
  if (!running) return;
  frame(Math.min(clock.getDelta(), 0.05));
  requestAnimationFrame(loop);
}
document.addEventListener('visibilitychange', () => {
  running = !document.hidden;
  if (running) { clock.getDelta(); requestAnimationFrame(loop); } else { playing?.pause(); playing = null; }
});

document.documentElement.classList.add('webgl-ready');
requestAnimationFrame(loop);
