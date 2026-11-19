# Landing page

The landing page of the Definitive Edition, served by Cloudflare Pages as <https://dw3-definitive.pages.dev/>. Static
files, deployed as they are; the only generated files are the translated pages.

| File | What it is |
|---|---|
| `index.html` | The English page, and the source of every other language: markup, inline CSS, the SEO head (meta, Open Graph, hreflang, JSON-LD `VideoGame` + `FAQPage`), and `#i18n-js`, the strings the scripts use |
| `pt-br/`, `fr/`, `de/`, `it/`, `es/` | Generated from `index.html` by `tools/site_i18n/build.py`. Do not edit by hand |
| `assets/app.js` | Scroll reveals, chapter tracking, the playable toys (combat, hunt chain, Blast gauge, compass), downloads, language menu |
| `assets/scene.js` | Three.js (jsDelivr, pinned to `three@0.170.0`): the world backdrop and the device that changes form as you scroll |
| `assets/shots/` | Screenshots from `docs/images/definitive/shots/`; the GIFs re-encoded to H.264 `.mp4` + `.jpg` poster |
| `robots.txt`, `sitemap.xml`, `site.webmanifest` | Indexing and install metadata (the sitemap lists every language with its alternates) |
| `_headers` | Cloudflare Pages cache and security headers |

## How the page works

- **Chapters.** Each `<section data-scene="…">` sets `--accent` in CSS and the 3D palette (`PALETTES` in `scene.js`).
- **The device.** A CRT TV with the 2002 console becomes a handheld at Combat, a phone at The hunt, a flat TV with a
  modern console at Story and a PC at Play anywhere (`FORMS` and `MORPH_AT` in `scene.js`). Its screen shows the
  chapter's footage and repeats what the toys do: `app.js` emits `dw3:pop`, `dw3:chain`, `dw3:rare`, `dw3:gauge`,
  `dw3:blast`. On wide screens it lives in the fixed `#devDock` and the chapters flow in a column to its left; on narrow
  screens it is drawn into each chapter's empty `.devslot`. Both exist only when `<head>` finds WebGL (`html.has-gl`);
  without it the page keeps a single-column layout, the CSS callouts and `assets/backdrop.jpg`.
- **Downloads.** The buttons link to the release files current when the page was edited (`data-asset`). On the visitor's
  side, `app.js` asks the GitHub API for the latest release and swaps in its files, and leads with the visitor's OS.
  When you publish a release, nothing has to change here; update the fallback links when convenient.
- **Languages.** The six languages of the README. The root page sends a first-time visitor to their browser's language
  (a choice in the language menu is remembered and wins; crawlers are not redirected), and every page carries
  `hreflang` alternates.

## Translations

`index.html` is the source. Text is split into units (an element with its own text and only inline markup, so a sentence
keeps its `<b>`/`<em>`), keyed by a hash of the English:

```fish
uv run tools/site_i18n/build.py extract   # index.html -> tools/site_i18n/en.json
uv run tools/site_i18n/build.py check     # per language: missing units, markup that differs
uv run tools/site_i18n/build.py build     # en.json + <lang>.json -> site/<lang>/index.html
```

After changing English text: `extract`, translate the new keys `check` reports as missing in each `<lang>.json` (keep
the tags and attributes of the English), then `build`. Until then those units show in English on the other pages.
Keep the visible FAQ in step with the `FAQPage` JSON-LD in `index.html`; the translated pages rebuild theirs from the
translated FAQ.

Re-encode a shot: `ffmpeg -i shot.gif -movflags +faststart -pix_fmt yuv420p -c:v libx264 -crf 27 -preset slow shot.mp4`.

Publish with `wrangler pages deploy site --project-name dw3-definitive --branch main` from the repository root.
