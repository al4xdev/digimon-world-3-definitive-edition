/* Landing page interactions. Plain script (no build step): reveal-on-scroll, chapter tracking,
 * and the small playable toys (combat, hunt chain, Blast gauge, compass). The 3D backdrop lives in
 * scene.js and listens for the "dw3:*" events dispatched here. */
(() => {
  'use strict';
  const $ = (s, r = document) => r.querySelector(s);
  const $$ = (s, r = document) => [...r.querySelectorAll(s)];
  const reduced = matchMedia('(prefers-reduced-motion: reduce)').matches;
  const emit = (name, detail) => window.dispatchEvent(new CustomEvent('dw3:' + name, { detail }));
  // Runtime strings live in #i18n-js so the page generator translates them with the rest of the page.
  let STR = {};
  try { STR = JSON.parse(document.getElementById('i18n-js').textContent); } catch {}
  const t = (key, vars) => String(STR[key] ?? key).replace(/\{(\w+)\}/g, (_, k) => (vars && k in vars ? vars[k] : ''));

  /* ---------- reveal + one-shot triggers ---------- */
  const once = new Map(); // element -> callback fired the first time it is on screen
  const io = new IntersectionObserver((entries) => {
    for (const e of entries) {
      if (!e.isIntersecting) continue;
      e.target.classList.add('in');
      const cb = once.get(e.target);
      if (cb) { once.delete(e.target); cb(e.target); }
      if (!once.has(e.target)) io.unobserve(e.target);
    }
  }, { rootMargin: '0px 0px -12% 0px', threshold: 0.12 });
  $$('.rv').forEach((el) => io.observe(el));
  const onFirstView = (el, cb) => { if (!el) return; once.set(el, cb); io.observe(el); };

  /* ---------- heading decode effect ---------- */
  // Re-types each [data-scramble] heading through random glyphs once. Text nodes only, so <br>/<em> survive.
  const GLYPHS = '!<>-_\\/[]{}=+*^?#01ABCDEFX';
  function scramble(el) {
    const nodes = [];
    const walk = (n) => n.nodeType === 3 ? nodes.push(n) : n.childNodes.forEach(walk);
    walk(el);
    const finals = nodes.map((n) => n.textContent);
    const start = performance.now();
    const dur = 900;
    const tick = (t) => {
      const p = Math.min(1, (t - start) / dur);
      nodes.forEach((n, i) => {
        const f = finals[i];
        const keep = Math.floor(f.length * p);
        let out = f.slice(0, keep);
        for (let k = keep; k < f.length; k++) out += f[k] === ' ' ? ' ' : GLYPHS[(Math.random() * GLYPHS.length) | 0];
        n.textContent = out;
      });
      if (p < 1) requestAnimationFrame(tick);
    };
    requestAnimationFrame(tick);
  }
  $$('[data-scramble]').forEach((el) => onFirstView(el, scramble));

  /* ---------- header, progress, chapter tracking ---------- */
  const header = $('header.top');
  const bar = $('.progress');
  let ticking = false;
  const onScroll = () => {
    if (ticking) return;
    ticking = true;
    requestAnimationFrame(() => {
      const y = scrollY;
      const max = document.documentElement.scrollHeight - innerHeight;
      header.classList.toggle('scrolled', y > 40);
      bar.style.transform = `scaleX(${max > 0 ? y / max : 0})`;
      ticking = false;
    });
  };
  addEventListener('scroll', onScroll, { passive: true });
  onScroll();

  const sections = $$('main > section[data-scene]');
  const railLinks = $$('.rail a');
  const navLinks = $$('.nav a');
  const chapterIO = new IntersectionObserver((entries) => {
    for (const e of entries) {
      if (!e.isIntersecting) continue;
      const s = e.target;
      document.body.dataset.scene = s.dataset.scene;
      emit('scene', s.dataset.scene);
      emit('chapter', s.id);
      railLinks.forEach((a) => a.classList.toggle('on', a.dataset.for === s.id || (s.id === 'hero' && a.dataset.for === 'main')));
      navLinks.forEach((a) => a.classList.toggle('on', a.getAttribute('href') === '#' + s.id));
    }
  }, { rootMargin: '-45% 0px -45% 0px' });
  sections.forEach((s) => chapterIO.observe(s));

  /* ---------- videos play only on screen ---------- */
  const vio = new IntersectionObserver((entries) => {
    for (const e of entries) {
      const v = e.target;
      if (e.isIntersecting) { v.preload = 'auto'; v.play().catch(() => {}); } else v.pause();
    }
  }, { threshold: 0.35 });
  $$('video').forEach((v) => vio.observe(v));

  /* ---------- count-ups ---------- */
  function countUp(el) {
    const to = +el.dataset.count;
    const t0 = performance.now(), dur = 1800;
    const step = (t) => {
      const p = Math.min(1, (t - t0) / dur);
      el.textContent = Math.round(to * (1 - Math.pow(1 - p, 3)));
      if (p < 1) requestAnimationFrame(step);
    };
    requestAnimationFrame(step);
  }
  $$('[data-count]').forEach((el) => { if (!el.closest('#race')) onFirstView(el, countUp); });
  onFirstView($('#race'), (r) => { r.classList.add('go'); $$('[data-count]', r).forEach(countUp); });

  /* ---------- chapter 01: the level counter that stalls at 40 ---------- */
  onFirstView($('#lvl'), (el) => {
    let lv = 1;
    // Fast at first, then slower and slower: the grind, felt.
    const next = () => {
      lv++;
      el.textContent = lv;
      if (lv < 40) setTimeout(next, 18 + Math.pow(lv / 40, 4) * 420);
    };
    setTimeout(next, 300);
  });

  /* ---------- chapter 02: the combat demo ---------- */
  const FAM = ['fire', 'water', 'wind', 'thunder', 'dark'].map((id) => ({ id, name: t('combat.fam.' + id), short: t('combat.short.' + id) }));
  // The demo foe: two weak families so the chain is discoverable, one resist to punish guessing.
  const AFF = { fire: 'resist', water: 'weak', wind: 'neutral', thunder: 'weak', dark: 'neutral' };
  const arena = $('#arena');
  if (arena) {
    const foe = $('#foe'), fx = $('#fx'), log = $('#log'), hpbar = $('#hpbar'), pips = $('#pips');
    const intel = $('#intel'), moves = $('#moves'), baton = $('#baton'), bits = $('#bits');
    const roundEl = $('#round'), partnerEl = $('#partner');
    const PARTNERS = ['Guilmon', 'Renamon'];
    let S;
    const reset = () => {
      S = { hp: 100, actions: 1, round: 1, stagger: false, paid: new Set(), known: {}, batonUsed: false, partner: 0, over: false, down: false, bits: 0, busy: false };
      foe.className = 'foe';
      bits.textContent = '';
      say(t('combat.start'));
      render();
    };
    const say = (t) => { log.textContent = t; };
    intel.innerHTML = FAM.map((f) => `<div data-f="${f.id}"><svg><use href="#i-${f.id}"/></svg>???</div>`).join('');
    moves.innerHTML = FAM.map((f) => `<button class="move" data-f="${f.id}" aria-label="${t('combat.tech', { name: f.name })}"><svg aria-hidden="true"><use href="#i-${f.id}"/></svg>${f.short}</button>`).join('');

    function render() {
      hpbar.style.width = Math.max(0, S.hp) + '%';
      pips.innerHTML = '<i></i>'.repeat(Math.max(0, S.actions));
      roundEl.textContent = S.round;
      partnerEl.textContent = PARTNERS[S.partner];
      foe.classList.toggle('stagger', S.stagger && !S.down);
      foe.classList.toggle('down', S.down);
      $$('div', intel).forEach((d) => {
        const k = S.known[d.dataset.f];
        d.className = k || '';
        d.lastChild.textContent = k ? ({ weak: t('combat.intel.weak'), resist: t('combat.intel.resist'), neutral: '—' })[k] : '???';
      });
      $$('.move', moves).forEach((b) => { b.disabled = S.busy || (S.down && !S.over); });
      baton.disabled = S.busy || !S.stagger || S.batonUsed || S.down;
      if (S.over || S.down) bits.textContent = S.bits ? t('combat.bits', { n: S.bits }) : '';
    }

    function pop(cls, text, delay = 0) {
      setTimeout(() => {
        const el = document.createElement('span');
        el.className = 'pop ' + cls;
        el.textContent = text;
        emit('pop', { cls, text }); // the 3D device's screen stamps the same callout
        el.style.top = (28 + Math.random() * 18) + '%';
        fx.appendChild(el);
        setTimeout(() => el.remove(), 1200);
      }, delay);
    }
    function dmgNum(n) {
      const el = document.createElement('span');
      el.className = 'dmg';
      el.textContent = n;
      el.style.left = (44 + Math.random() * 14) + '%';
      el.style.top = '40%';
      fx.appendChild(el);
      setTimeout(() => el.remove(), 900);
    }

    function hit(f) {
      if (S.busy) return;
      // A downed foe that stayed up for OVERFINISH: every hit pays Bits.
      if (S.down) {
        if (!S.over) return;
        const b = 40 + ((Math.random() * 30) | 0);
        S.bits += b;
        foe.classList.remove('hit'); void foe.offsetWidth; foe.classList.add('hit');
        pop('over', t('pop.over'));
        dmgNum(t('combat.bits', { n: b }));
        say(t('combat.overHit'));
        S.over = false;
        S.busy = true;
        render();
        setTimeout(() => { S.busy = false; say(t('combat.victory')); render(); }, 1300);
        return;
      }
      const a = AFF[f];
      const firstWeak = a === 'weak' && !S.paid.has(f);
      let dmg;
      S.known[f] = a;
      S.actions--;
      if (a === 'weak') {
        dmg = S.stagger ? 34 : 24;
        pop('weak', t('pop.weak'));
        if (firstWeak) {
          S.paid.add(f);
          S.stagger = true;
          S.actions++;
          pop('more', t('pop.more'), 380);
          say(t('combat.weakFound', { name: FAM.find((x) => x.id === f).name }));
        } else {
          say(t('combat.weakPaid'));
        }
        arena.classList.remove('flash'); void arena.offsetWidth; arena.classList.add('flash');
        emit('weak');
      } else if (a === 'resist') {
        dmg = 6;
        pop('resist', t('pop.resist'));
        say(S.stagger ? t('combat.resistStagger') : t('combat.resist'));
        S.stagger = false;
      } else {
        dmg = S.stagger ? 18 : 12;
        pop('plain', S.stagger ? t('pop.bonus') : t('pop.hit'));
        say(S.stagger ? t('combat.bonus') : t('combat.neutral'));
        S.stagger = false;
      }
      S.hp -= dmg;
      dmgNum(dmg);
      foe.classList.remove('hit'); void foe.offsetWidth; foe.classList.add('hit');

      if (S.hp <= 0) {
        S.hp = 0;
        S.down = true;
        S.over = firstWeak; // the KO blow earned one more action -> OVERFINISH window
        S.stagger = false;
        say(S.over ? t('combat.koOver') : t('combat.ko'));
        if (!S.over) setTimeout(() => say(t('combat.victory')), 1600);
        render();
        return;
      }
      if (S.actions <= 0) enemyTurn();
      render();
    }

    function enemyTurn() {
      S.busy = true;
      render();
      setTimeout(() => {
        say(t('combat.enemy'));
        if (!reduced) arena.animate?.([{ transform: 'translateX(0)' }, { transform: 'translateX(-6px)' }, { transform: 'translateX(5px)' }, { transform: 'none' }], { duration: 320 });
      }, 700);
      setTimeout(() => {
        S.round++;
        S.actions = 1;
        S.stagger = false;
        S.paid.clear();
        S.batonUsed = false;
        S.busy = false;
        say(t('combat.newRound'));
        render();
      }, 1700);
    }

    moves.addEventListener('click', (e) => { const b = e.target.closest('.move'); if (b) hit(b.dataset.f); });
    baton.addEventListener('click', () => {
      if (baton.disabled) return;
      S.batonUsed = true;
      S.partner ^= 1;
      pop('baton', t('pop.baton'));
      say(t('combat.batonIn', { name: PARTNERS[S.partner] }));
      render();
    });
    $('#reset').addEventListener('click', reset);
    reset();
  }

  /* ---------- chapter 04: hunt chain ---------- */
  const pipsBox = $('#chainPips');
  if (pipsBox) {
    // Real curve (docs/definitive-edition/hunt-and-economy.md): 0% up to 5 wins, 20% at 6, +20% per win, sure at 10.
    const chance = (n) => (n <= 5 ? 0 : Math.min(100, (n - 5) * 20));
    pipsBox.innerHTML = Array.from({ length: 10 }, (_, i) => `<i class="${i >= 5 ? 'hot' : ''}" data-p="${chance(i + 1) ? chance(i + 1) + '%' : ''}"></i>`).join('');
    let n = 0;
    const win = $('#winBtn'), odds = $('#odds'), rare = $('#rare');
    const draw = () => {
      $$('i', pipsBox).forEach((p, i) => p.classList.toggle('on', i < n));
      odds.textContent = chance(n + 1) + '%';
      emit('chain', n);
    };
    win.addEventListener('click', () => {
      // The next encounter rolls against the chance for the chain it would reach.
      const c = chance(n + 1);
      n = Math.min(10, n + 1);
      draw();
      if (c && Math.random() * 100 < c) {
        rare.classList.add('show');
        win.disabled = true;
        odds.textContent = t('hunt.found');
        emit('weak');
        emit('rare', true);
      }
    });
    $('#huntAgain').addEventListener('click', () => { n = 0; rare.classList.remove('show'); win.disabled = false; emit('rare', false); draw(); });
    draw();
  }

  /* ---------- chapter 06: hero toggle ---------- */
  const toggle = $('#heroToggle');
  if (toggle) {
    const LINES = { kail: t('hero.kail'), junior: t('hero.junior') };
    const set = (v) => {
      toggle.dataset.v = v;
      $$('button', toggle).forEach((b) => b.setAttribute('aria-pressed', String(b.dataset.v === v)));
      $('#heroLine').innerHTML = LINES[v];
    };
    toggle.addEventListener('click', (e) => { const b = e.target.closest('button'); if (b) set(b.dataset.v); });
    set('kail');
  }

  /* ---------- chapter 07: Blast gauge ---------- */
  const gauge = $('#gauge');
  if (gauge) {
    let g = 0;
    const barI = $('#gaugeBar'), r2 = $('#r2'), price = $('#price'), meter = $('.gauge', gauge);
    const draw = () => {
      barI.style.width = g + '%';
      meter.setAttribute('aria-valuenow', g);
      gauge.classList.toggle('full', g >= 100);
      r2.disabled = g < 100;
      emit('gauge', g);
    };
    $('.feed', gauge).addEventListener('click', (e) => {
      const b = e.target.closest('button');
      if (!b) return;
      g = Math.min(100, g + +b.dataset.g);
      price.textContent = g >= 100 ? t('blast.full') : '';
      draw();
    });
    const unleash = () => {
      if (g < 100) return;
      g = 0;
      draw();
      if (!reduced) { gauge.classList.remove('unleashed'); void gauge.offsetWidth; gauge.classList.add('unleashed'); }
      const fl = $('.blastflash');
      fl.classList.remove('go'); void fl.offsetWidth; fl.classList.add('go');
      price.textContent = t('blast.price');
      emit('blast');
    };
    r2.addEventListener('click', unleash);
    // A real R2 on a gamepad would be nice, but keyboard "R" is enough for the joke.
    addEventListener('keydown', (e) => { if ((e.key === 'r' || e.key === 'R') && !e.target.closest('input,textarea') && document.body.dataset.scene === 'blast') unleash(); });
    draw();
  }

  /* ---------- chapter 07: Super Rare compass ---------- */
  const comp = $('#compass');
  if (comp) {
    const temp = $('#temp'), hint = $('#chint'), tbars = $('#tbars');
    tbars.innerHTML = [8, 12, 16, 20, 24].map((h) => `<i style="height:${h}px"></i>`).join('');
    const LEVELS = [
      [t('compass.cold'), '#4aa3ff', 'rgba(74,163,255,.25)'],
      [t('compass.cool'), '#7cc4ff', 'rgba(124,196,255,.25)'],
      [t('compass.warm'), '#f3c843', 'rgba(243,200,67,.28)'],
      [t('compass.hot'), '#ef6a3a', 'rgba(239,106,58,.32)'],
      [t('compass.burning'), '#ff3b5c', 'rgba(255,59,92,.4)'],
    ];
    let tx, ty, found = false, flees = 0;
    const place = () => { tx = 12 + Math.random() * 76; ty = 18 + Math.random() * 64; };
    place();
    const target = $('#ctarget');
    const sense = (cx, cy) => {
      if (found) return;
      const r = comp.getBoundingClientRect();
      const x = ((cx - r.left) / r.width) * 100, y = ((cy - r.top) / r.height) * 100;
      const d = Math.hypot(x - tx, (y - ty) * (r.height / r.width));
      const lv = d < 7 ? 4 : d < 16 ? 3 : d < 28 ? 2 : d < 42 ? 1 : 0;
      const [name, col, heat] = LEVELS[lv];
      temp.textContent = name;
      temp.style.color = col;
      comp.style.setProperty('--tx', x + '%');
      comp.style.setProperty('--ty', y + '%');
      comp.style.setProperty('--heat', heat);
      $$('i', tbars).forEach((b, i) => { b.style.background = i <= lv ? col : ''; });
      if (lv === 4) {
        found = true;
        target.style.left = tx + '%';
        target.style.top = ty + '%';
        comp.classList.add('found');
        flees++;
        hint.textContent = flees < 3 ? t('compass.flee', { n: 20 * flees }) : t('compass.cornered');
        setTimeout(() => {
          comp.classList.remove('found');
          if (flees >= 3) flees = 0;
          place();
          found = false;
          hint.textContent = t('compass.again');
        }, 2400);
      }
    };
    comp.addEventListener('pointermove', (e) => sense(e.clientX, e.clientY));
    comp.addEventListener('pointerdown', (e) => sense(e.clientX, e.clientY));
  }

  /* ---------- Velvet guest cards: spotlight follows the pointer ---------- */
  $$('.guest').forEach((c) => {
    c.addEventListener('pointermove', (e) => {
      const r = c.getBoundingClientRect();
      const px = (e.clientX - r.left) / r.width, py = (e.clientY - r.top) / r.height;
      c.style.setProperty('--mx', px * 100 + '%');
      c.style.setProperty('--my', py * 100 + '%');
      if (!reduced && e.pointerType === 'mouse') c.style.transform = `rotateX(${(0.5 - py) * 10}deg) rotateY(${(px - 0.5) * 12}deg) translateZ(0)`;
    });
    c.addEventListener('pointerleave', () => { c.style.transform = ''; });
  });

  /* ---------- get started: tabs ---------- */
  const tabs = $$('[role="tab"]');
  const selectTab = (t) => {
    tabs.forEach((x) => {
      const on = x === t;
      x.setAttribute('aria-selected', String(on));
      x.tabIndex = on ? 0 : -1;
      $('#' + x.getAttribute('aria-controls')).hidden = !on;
    });
    emit('platform', t.id); // the 3D device becomes a PC, a phone or a handheld to match
  };
  tabs.forEach((t, i) => {
    t.addEventListener('click', () => selectTab(t));
    t.addEventListener('keydown', (e) => {
      const d = e.key === 'ArrowRight' ? 1 : e.key === 'ArrowLeft' ? -1 : 0;
      if (!d) return;
      const n = tabs[(i + d + tabs.length) % tabs.length];
      selectTab(n); n.focus();
    });
  });
  /* ---------- downloads: the visitor's platform first, the newest release's files ---------- */
  const ua = navigator.userAgent;
  const uaPlat = navigator.userAgentData?.platform || navigator.platform || '';
  const OS = /Android/i.test(ua) ? 'android'
    : /iPhone|iPad|iPod/i.test(ua) ? 'ios'
    : /Win/i.test(uaPlat) || /Windows/i.test(ua) ? 'windows'
    : /Mac/i.test(uaPlat) ? (navigator.maxTouchPoints > 1 ? 'ios' : 'mac')
    : /Linux|X11|CrOS/i.test(uaPlat + ua) ? 'linux' : '';
  if (OS === 'android') selectTab($('#t-and'));
  if (OS === 'mac' || OS === 'ios') { const n = $('#osNote'); if (n) n.hidden = false; }
  // On a PC, lead with the visitor's own system.
  const win = $('[data-asset="windows"]'), lin = $('[data-asset="linux"]');
  if (win && lin && (OS === 'windows' || OS === 'linux')) {
    const [mine, other] = OS === 'windows' ? [win, lin] : [lin, win];
    mine.className = 'btn primary rec';
    other.className = 'btn ghost';
    mine.parentNode.prepend(mine);
  }
  // The hero CTA names the platform when it is one we ship for.
  const OS_NAME = { android: 'Android', windows: 'Windows', linux: 'Linux' };
  const heroCta = $('.hero .actions .btn.primary');
  if (heroCta && OS_NAME[OS]) heroCta.lastChild.textContent = t('cta.getFor', { os: OS_NAME[OS] });

  // Release asset names carry the version, so match them by suffix.
  const ASSET = {
    windows: /windows-x86_64\.zip$/, linux: /x86_64\.AppImage$/, android: /android-arm64-v8a\.apk$/, portmaster: /^dw3\.zip$/,
  };
  const applyRelease = (rel) => {
    if (!rel || !rel.tag_name || !Array.isArray(rel.assets)) return;
    for (const [key, re] of Object.entries(ASSET)) {
      const a = rel.assets.find((x) => re.test(x.name));
      const el = $(`[data-asset="${key}"]`);
      if (a && el) el.href = a.browser_download_url;
    }
    const tag = $('#relTag'), link = $('#relLink'), info = $('#relInfo');
    if (tag) tag.textContent = rel.tag_name;
    if (info) info.hidden = false;
    if (link && rel.html_url) link.href = rel.html_url;
  };
  // Fetched once the download section is near, cached for the session: the unauthenticated API allows 60 calls an hour per IP.
  onFirstView($('#get'), async () => {
    const KEY = 'dw3-rel';
    try {
      const c = JSON.parse(sessionStorage.getItem(KEY) || 'null');
      if (c && Date.now() - c.at < 36e5) return applyRelease(c.rel);
    } catch {}
    try {
      const r = await fetch('https://api.github.com/repos/al4xdev/digimon-world-3-definitive-edition/releases/latest', { headers: { Accept: 'application/vnd.github+json' } });
      if (!r.ok) return;
      const j = await r.json();
      const rel = { tag_name: j.tag_name, html_url: j.html_url, assets: (j.assets || []).map((x) => ({ name: x.name, browser_download_url: x.browser_download_url })) };
      applyRelease(rel);
      try { sessionStorage.setItem(KEY, JSON.stringify({ at: Date.now(), rel })); } catch {}
    } catch {}
  });

  /* ---------- language menu: an explicit pick is remembered, so the root page stops redirecting ---------- */
  $$('a[data-lang]').forEach((a) => a.addEventListener('click', () => { try { localStorage.setItem('dw3-lang', a.dataset.lang); } catch {} }));
  const langMenu = $('#lang');
  document.addEventListener('click', (e) => { if (langMenu?.open && !langMenu.contains(e.target)) langMenu.open = false; });
  document.addEventListener('keydown', (e) => { if (e.key === 'Escape' && langMenu?.open) { langMenu.open = false; langMenu.querySelector('summary').focus(); } });

  const copy = $('#copySha');
  copy?.addEventListener('click', async () => {
    const label = copy.textContent;
    try { await navigator.clipboard.writeText($('#sha').textContent.trim()); copy.textContent = t('copy.done'); }
    catch { copy.textContent = t('copy.fail'); }
    setTimeout(() => { copy.textContent = label; }, 1600);
  });
})();
