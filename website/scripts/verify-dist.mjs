import { existsSync, readdirSync, readFileSync } from 'node:fs';
import { join, resolve } from 'node:path';

const DIST = resolve(process.cwd(), 'website/dist');

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
  'images/build-v4/v4-step-1-unbox-pcba.png',
  'images/build-v4/v4-step-2-plug-modules.png',
  'images/build-v4/v4-step-3-enclosure-screws.png',
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

// Count legacy images across the 5 original subdirectories (must be 93)
let legacyImgCount = 0;
for (const dir of ['device', 'manual', 'software', 'thermal', 'website']) {
  const full = join(DIST, 'images', dir);
  if (existsSync(full)) {
    legacyImgCount += readdirSync(full).filter((f) => !f.startsWith('.')).length;
  }
}
if (legacyImgCount !== 93) {
  errors.push(`Expected 93 legacy images, found ${legacyImgCount}`);
}

// Verify BOM rows rendered in /docs/partlist/index.html
const partlistHtml = readFileSync(join(DIST, 'docs/partlist/index.html'), 'utf8');
for (const needle of ['FLIR Lepton 3.5', 'Teensy 4.1', 'ER-TFTM032-3', 'U3V16F5', 'BQ24075']) {
  if (!partlistHtml.includes(needle)) {
    errors.push(`Missing BOM needle in /docs/partlist/: ${needle}`);
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
  `VERIFICATION PASSED: ${REQUIRED_ROUTES.length} routes, ${legacyImgCount} legacy images, ${REQUIRED_ASSETS.length} new/meta assets, BOM rows, Umami, OG, JSON-LD, and Pagefind all verified.`
);
