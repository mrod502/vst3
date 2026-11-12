#!/usr/bin/env node
/**
 * Parse-check every mermaid block in a markdown file, or a whole .mmd file.
 *
 *   node scripts/check_diagrams.mjs plan/M5-impl-plan/INDEX.md templates/dependency-chart.mmd
 *
 * Exit 0 when every block parses, 1 when any fails or a markdown file has none.
 *
 * Dev-only tooling: it deliberately lives outside the CMake build and is not a CTest
 * case. It needs mermaid and jsdom, which are not repo dependencies — install them once
 * in a scratch directory and run it from there (module resolution walks up from the
 * working directory, so `cd /tmp/mmcheck` is what makes the imports resolve):
 *
 *   mkdir -p /tmp/mmcheck && cd /tmp/mmcheck && npm init -y && npm i mermaid jsdom
 *   node /path/to/skill/scripts/check_diagrams.mjs /workspace/plan/M5-impl-plan/INDEX.md
 *
 * Note on comments: mermaid 12's parse() does not strip `%%` lines that precede the
 * diagram type, so a .mmd template kept comment-free will parse and one headed by a
 * comment block will not. Keep the templates clean and the rules in SKILL.md.
 */

import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

/** Resolve a package's entry point by walking up from the working directory, so the
 *  scratch directory that holds node_modules does not have to be the script's home. */
function entryUrl(name) {
  let dir = process.cwd();
  for (;;) {
    const pkgDir = path.join(dir, 'node_modules', name);
    if (fs.existsSync(pkgDir)) {
      const pkg = JSON.parse(fs.readFileSync(path.join(pkgDir, 'package.json'), 'utf8'));
      const exported = pkg.exports?.['.'];
      const rel = (typeof exported === 'string' ? exported
        : exported?.import ?? exported?.default) ?? pkg.module ?? pkg.main ?? 'index.js';
      return pathToFileURL(path.join(pkgDir, rel)).href;
    }
    const parent = path.dirname(dir);
    if (parent === dir) break;
    dir = parent;
  }
  throw new Error(`cannot resolve '${name}' from ${process.cwd()} — ` +
    'install it (npm i mermaid jsdom) and re-run from that directory');
}

const { JSDOM } = await import(entryUrl('jsdom'));

const dom = new JSDOM('<!DOCTYPE html><html><body></body></html>', { pretendToBeVisual: true });
const define = (key, value) => {
  try {
    Object.defineProperty(globalThis, key, { value, configurable: true, writable: true });
  } catch {
    /* node owns this global and will not give it up; mermaid reads the real one */
  }
};
define('window', dom.window);
define('document', dom.window.document);
for (const key of ['Element', 'HTMLElement', 'SVGElement', 'Node', 'DOMParser', 'XMLSerializer',
  'getComputedStyle', 'requestAnimationFrame', 'cancelAnimationFrame', 'MutationObserver',
  'CustomEvent', 'Event']) {
  define(key, dom.window[key]);
}

const mermaid = (await import(entryUrl('mermaid'))).default;
mermaid.initialize({ startOnLoad: false, securityLevel: 'loose' });

if (process.argv.length < 3) {
  console.error('usage: check_diagrams.mjs <file.md | file.mmd> [...]');
  process.exit(2);
}

let failures = 0;
for (const path of process.argv.slice(2)) {
  const raw = fs.readFileSync(path, 'utf8');
  const blocks = [];
  if (path.endsWith('.mmd')) {
    blocks.push(['(whole file)', raw]);
  } else {
    const fence = /```mermaid\r?\n([\s\S]*?)```/g;
    let match;
    let index = 0;
    while ((match = fence.exec(raw))) blocks.push([`fence #${++index}`, match[1]]);
  }
  if (blocks.length === 0) {
    console.log(`FAIL ${path}: no mermaid block found`);
    failures++;
    continue;
  }
  for (const [label, text] of blocks) {
    try {
      const result = await mermaid.parse(text);
      const lines = text.split('\n').length;
      console.log(`PASS ${path} ${label} (diagramType=${result?.diagramType ?? '?'}, ${lines} lines)`);
    } catch (error) {
      failures++;
      const detail = String(error?.message ?? error).split('\n').slice(0, 6).join('\n  ');
      console.log(`FAIL ${path} ${label}\n  ${detail}`);
    }
  }
}
process.exit(failures ? 1 : 0);
