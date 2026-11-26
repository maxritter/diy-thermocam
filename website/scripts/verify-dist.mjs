import { existsSync, readdirSync, readFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const DIST = resolve(__dirname, '../dist');

const REQUIRED_ROUTES = [
  'index.html',
  'building-instructions/index.html',
  'device-features/index.html',
  'device-images/index.html',
  'software/index.html',
  'thermal-images/index.html',
  'docs/index.html',
  'docs/manual/index.html',
  'docs/hardware/index.html',
  'docs/partlist/index.html',
  'docs/firmware/index.html',
  'docs/raw-data/index.html',
  'docs/serial/index.html',
  'docs/protocol-v2/index.html',
  'docs/desktop-app/index.html',
];

const REQUIRED_ASSETS = [
  'sitemap-index.xml',
  'robots.txt',
  'favicon.svg',
  'pagefind/pagefind.js',
  'images/og/github-social-preview.png',
  'images/og/og-default.png',
  'images/brand/logo-mark.svg',
  'images/brand/logo-light.svg',
  'images/brand/logo-dark.svg',
  'images/brand/favicon.png',
  'images/brand/hero-thermal-banner.png',
  'images/thermal/real-v3-1080p-capture.png',
  'images/v4/pcb-modules.png',
  'images/v4/pcb-lepton.png',
  'images/v4/fr4-battery.png',
];

const errors = [];

for (const route of REQUIRED_ROUTES) {
  if (!existsSync(join(DIST, route))) {
    errors.push(`Missing route: ${route}`);
  }
}

for (const asset of REQUIRED_ASSETS) {
  if (!existsSync(join(DIST, asset))) {
    errors.push(`Missing asset: ${asset}`);
  }
}

// Count gallery/device/manual images across the 5 canonical subdirectories (93 legacy + real-v3-1080p-capture.png)
let legacyImgCount = 0;
for (const dir of ['device', 'manual', 'software', 'thermal', 'website']) {
  const full = join(DIST, 'images', dir);
  if (existsSync(full)) {
    legacyImgCount += readdirSync(full).filter((f) => !f.startsWith('.')).length;
  }
}
if (legacyImgCount < 93) {
  errors.push(`Expected at least 93 gallery/device images, found ${legacyImgCount}`);
}

// Verify the parts list ships its parts in the HTML, before any script runs
const partlistHtml = readFileSync(join(DIST, 'docs/partlist/index.html'), 'utf8');
for (const needle of ['FLIR Lepton 3.5', 'Teensy 4.1', 'ER-TFTM032-3', 'Adafruit 258', 'C585422']) {
  if (!partlistHtml.includes(needle)) {
    errors.push(`Missing part in /docs/partlist/: ${needle}`);
  }
}

// Verify Umami, JSON-LD, OG tags, and absence of legacy Stackbit strings
for (const route of REQUIRED_ROUTES) {
  const html = readFileSync(join(DIST, route), 'utf8');
  if (!html.includes('efb7f48a-b0e1-46ff-9e74-2a2c1a76e868')) {
    errors.push(`Missing Umami ID in ${route}`);
  }
  if (!html.includes('og-default.png')) {
    errors.push(`Missing og-default.png meta in ${route}`);
  }
  for (const banned of ['Welcome to Libris', 'A personal theme for Unibit', 'href="/images/icon.png"']) {
    if (html.includes(banned)) {
      errors.push(`Found legacy bug string "${banned}" in ${route}`);
    }
  }
}

for (const ldRoute of ['index.html', 'building-instructions/index.html', 'software/index.html', 'device-features/index.html']) {
  const html = readFileSync(join(DIST, ldRoute), 'utf8');
  if (!html.includes('application/ld+json')) {
    errors.push(`Missing JSON-LD in ${ldRoute}`);
  }
}

if (errors.length > 0) {
  console.error('VERIFICATION FAILED:\n' + errors.map((e) => ` - ${e}`).join('\n'));
  process.exit(1);
}

console.log(
  `VERIFICATION PASSED: ${REQUIRED_ROUTES.length} routes, ${legacyImgCount} gallery/device images, ${REQUIRED_ASSETS.length} brand/build/meta assets, parts list, Umami, OG, JSON-LD, and Pagefind all verified.`
);
