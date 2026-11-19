# /// script
# requires-python = ">=3.10"
# dependencies = ["beautifulsoup4>=4.12"]
# ///
"""Landing page translations: site/index.html (English) is the source, one page per README language.

    uv run tools/site_i18n/build.py extract   # site/index.html -> tools/site_i18n/en.json
    uv run tools/site_i18n/build.py build     # en.json + <lang>.json -> site/<lang>/index.html
    uv run tools/site_i18n/build.py check     # report missing / stale keys per language

There are no hand-written keys. A "unit" is an element that holds text itself and only inline markup
(<b>, <em>, <code>, <a>, <br>...), so a sentence travels whole with its emphasis; its key is a hash of the
English HTML. When English text changes, its key changes and every language falls back to English for that
unit until it is translated again: `check` lists those. Elements that also hold block content (an <svg>, a
<div>) are translated text node by text node instead. Attributes (alt, aria-label, title, data-rec), the
<head> metadata and the runtime string table (#i18n-js, keyed "js:<name>") are units too.
Anything under translate="no" is left alone.
"""
from __future__ import annotations

import copy
import hashlib
import json
import re
import sys
from pathlib import Path

from bs4 import BeautifulSoup, Comment, NavigableString, Tag

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "site" / "index.html"
DIR = Path(__file__).resolve().parent
ORIGIN = "https://dw3-definitive.pages.dev"

# path segment, html lang, og:locale, JSON-LD inLanguage
LANGS = {
    "pt-br": ("pt-BR", "pt_BR"),
    "fr": ("fr", "fr_FR"),
    "de": ("de", "de_DE"),
    "it": ("it", "it_IT"),
    "es": ("es", "es_ES"),
}
INLINE = {"b", "i", "em", "strong", "code", "br", "small", "a", "span", "kbd", "sup", "sub", "abbr"}
SKIP = {"script", "style", "svg", "noscript", "template"}
ATTRS = ("alt", "aria-label", "title", "data-rec")
META = {  # selector -> attribute holding the text
    'meta[name="description"]': "content",
    'meta[property="og:title"]': "content",
    'meta[property="og:description"]': "content",
    'meta[property="og:image:alt"]': "content",
    'meta[name="twitter:title"]': "content",
    'meta[name="twitter:description"]': "content",
    'meta[name="twitter:image:alt"]': "content",
}
LETTER = re.compile(r"[^\W\d_]", re.UNICODE)


def norm(s: str) -> str:
    return re.sub(r"\s+", " ", s).strip()


def key_of(s: str) -> str:
    return hashlib.sha1(norm(s).encode()).hexdigest()[:10]


def inner(el: Tag) -> str:
    return norm("".join(str(c) for c in el.contents))


def inline_only(el: Tag) -> bool:
    for c in el.children:
        if isinstance(c, Comment):
            continue
        if isinstance(c, Tag) and (c.name not in INLINE or not inline_only(c)):
            return False
    return True


def no_translate(el: Tag) -> bool:
    return any(p.get("translate") == "no" for p in [el, *el.parents] if isinstance(p, Tag))


def units(soup: BeautifulSoup):
    """Yield (key, english, setter) for every translatable piece of the page, in page order."""
    title = soup.title
    yield key_of(title.string), norm(title.string), lambda v, t=title: t.string.replace_with(v)
    for sel, attr in META.items():
        el = soup.select_one(sel)
        if el is not None:
            yield key_of(el[attr]), norm(el[attr]), lambda v, e=el, a=attr: e.__setitem__(a, v)
    for ld in soup.find_all("script", type="application/ld+json"):
        data = json.loads(ld.string)
        if "VideoGame" in data.get("@type", []):
            yield key_of(data["description"]), norm(data["description"]), None  # applied in build()

    def walk(el: Tag):
        if el.name in SKIP or el.get("translate") == "no":
            return
        for attr in ATTRS:
            v = el.get(attr)
            if v and LETTER.search(v):
                yield key_of(v), norm(v), lambda val, e=el, a=attr: e.__setitem__(a, val)
        direct = [c for c in el.children if isinstance(c, NavigableString) and not isinstance(c, Comment) and LETTER.search(c)]
        if direct and inline_only(el):
            html = inner(el)
            yield key_of(html), html, lambda val, e=el: replace_inner(e, val)
            # attributes of inline children (e.g. a title on an <a>) still count
            for c in el.find_all(True):
                for attr in ATTRS:
                    v = c.get(attr)
                    if v and LETTER.search(v):
                        yield key_of(v), norm(v), lambda val, e=c, a=attr: e.__setitem__(a, val)
            return
        for c in list(el.children):
            if isinstance(c, Tag):
                yield from walk(c)
            elif c in direct:
                text = norm(c)
                lead = " " if c[:1].isspace() else ""
                trail = " " if c[-1:].isspace() else ""
                yield key_of(text), text, lambda val, n=c, l=lead, t=trail: n.replace_with(l + val + t)

    yield from walk(soup.body)

    js = soup.find("script", id="i18n-js")
    table = json.loads(js.string)
    for name, v in table.items():
        yield "js:" + name, v, None  # applied in build()


def replace_inner(el: Tag, html: str):
    el.clear()
    frag = BeautifulSoup(html, "html.parser")
    for c in list(frag.contents):
        el.append(c)


def load(name: str) -> dict:
    p = DIR / f"{name}.json"
    return json.loads(p.read_text()) if p.exists() else {}


def extract():
    soup = BeautifulSoup(SRC.read_text(), "html.parser")
    out: dict[str, str] = {}
    for k, en, _ in units(soup):
        out.setdefault(k, en)
    (DIR / "en.json").write_text(json.dumps(out, ensure_ascii=False, indent=1) + "\n")
    print(f"en.json: {len(out)} units")


def check():
    en = load("en")
    bad = False
    for lang in LANGS:
        tr = load(lang)
        missing = [k for k in en if k not in tr]
        stale = [k for k in tr if k not in en]
        tags = [k for k in en if k in tr and sorted(re.findall(r"<(\w+)", en[k])) != sorted(re.findall(r"<(\w+)", tr[k]))]
        print(f"{lang}: {len(en) - len(missing)}/{len(en)} translated, {len(stale)} stale, {len(tags)} with different markup")
        for k in tags:
            print(f"   markup differs {k}: {en[k][:70]!r}")
        bad |= bool(missing or tags)
    return 1 if bad else 0


def build():
    src = SRC.read_text()
    en = load("en")
    for lang, (hl, locale) in LANGS.items():
        tr = load(lang)
        T = lambda k, fallback: tr.get(k, fallback)  # noqa: E731
        soup = BeautifulSoup(src, "html.parser")
        seen = set()
        for k, english, setter in list(units(soup)):
            if setter is not None and k in tr:
                setter(tr[k])
            seen.add(k)
        url = f"{ORIGIN}/{lang}/"
        soup.html["lang"] = hl
        soup.find("link", rel="canonical")["href"] = url
        soup.find("meta", property="og:url")["content"] = url
        soup.find("meta", property="og:locale")["content"] = locale
        soup.find("script", id="langRedirect").decompose()
        # the menu marks the current page and shows its code
        for a in soup.select("#lang a[data-lang]"):
            if a.get("data-lang") == lang:
                a["aria-current"] = "page"
            elif a.has_attr("aria-current"):
                del a["aria-current"]
        soup.select_one("#lang summary span").string = lang.upper()
        # runtime strings
        js = soup.find("script", id="i18n-js")
        table = json.loads(js.string)
        table = {n: T("js:" + n, v) for n, v in table.items()}
        js.string = "\n" + json.dumps(table, ensure_ascii=False, indent=0) + "\n"
        # JSON-LD: the game's description, and the FAQ rebuilt from the translated <details>
        for ld in soup.find_all("script", type="application/ld+json"):
            data = json.loads(ld.string)
            if "VideoGame" in data.get("@type", []):
                data["description"] = T(key_of(data["description"]), data["description"])
                data["url"] = url
            elif data.get("@type") == "FAQPage":
                data["mainEntity"] = [
                    {"@type": "Question", "name": d.summary.get_text(" ", strip=True),
                     "acceptedAnswer": {"@type": "Answer", "text": d.p.get_text(" ", strip=True)}}
                    for d in soup.select("#faq details")
                ]
            ld.string = "\n" + json.dumps(data, ensure_ascii=False, indent=2) + "\n"
        out = ROOT / "site" / lang / "index.html"
        out.parent.mkdir(exist_ok=True)
        html = str(soup)
        if not html.lstrip().lower().startswith("<!doctype"):
            html = "<!doctype html>\n" + html
        out.write_text(html)
        n = sum(1 for k in en if k in tr)
        print(f"site/{lang}/index.html  ({n}/{len(en)} units translated)")


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "build"
    sys.exit({"extract": extract, "build": build, "check": check}[cmd]() or 0)
