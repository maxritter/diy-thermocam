import { defineCollection } from 'astro:content';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { docsLoader } from '@astrojs/starlight/loaders';
import { docsSchema } from '@astrojs/starlight/schema';
import type { Loader } from 'astro/loaders';

interface RepoDocSpec {
  id: string;
  sourceFile: string;
  title: string;
  description: string;
  preamble?: string;
}

const REPO_DOCS: RepoDocSpec[] = [
  {
    id: 'docs/manual',
    sourceFile: 'manual.md',
    title: 'User Manual',
    description:
      'Daily operation, touchscreen controls, charging, live mode, settings, and firmware updates for DIY-Thermocam V3.',
  },
  {
    id: 'docs/hardware',
    sourceFile: 'hardware.md',
    title: 'Hardware, PCB & Enclosure',
    description:
      'Schematic, Teensy 4.1 pinmap, KiCad V3.0 and V4.0 Zero-Solder PCBA revisions, and build123d laser-cut and 3D-printable enclosures.',
  },
  {
    id: 'docs/firmware',
    sourceFile: 'firmware.md',
    title: 'Firmware & Flashing',
    description:
      'Flashing prebuilt firmware with the Desktop App or Teensy Loader, and building from source with arduino-cli and Teensy core 1.62.0.',
  },
  {
    id: 'docs/raw-data',
    sourceFile: 'raw-data-format.md',
    title: 'Raw Data Format (.DAT)',
    description:
      'Byte-exact specification of radiometric .DAT files across DIY-Thermocam V1, V2, and V3, plus calibration formulas and .thermo.json sidecars.',
  },
  {
    id: 'docs/protocol-v2',
    sourceFile: 'protocol-v2.md',
    title: 'USB Protocol v2.0 Specification',
    description:
      'Authoritative binary USB High-Speed wire protocol for DIY-Thermocam V3 firmware 3.1+ and the Tauri v2 Desktop App.',
  },
  {
    id: 'docs/serial',
    sourceFile: 'protocol-v2.md',
    title: 'USB Serial Protocol',
    description:
      'Communicating with DIY-Thermocam V3 over USB CDC ACM using binary Protocol v2.0 (replacing legacy single-byte v1 serial).',
    preamble:
      '> **Protocol v2 Migration Note:** Firmware 3.1+ replaces the legacy v1 single-byte serial protocol with **USB Protocol v2.0** (full-duplex little-endian binary framing, CRC-8/CRC-32 integrity, push streaming, and direct SD card access without MTP). This page stays live at `/docs/serial/` for existing bookmarks and mirrors the authoritative [USB Protocol v2.0 Specification](/docs/protocol-v2/) directly from `docs/protocol-v2.md`.\n\n',
  },
  {
    id: 'docs/desktop-app',
    sourceFile: 'software.md',
    title: 'Desktop App & Software',
    description:
      'Cross-platform Tauri v2 Desktop App for live WebGL2 viewing, radiometric ROIs, emissivity correction, H.264 MP4 export, SD browsing, and HalfKay updates.',
  },
];

function stripLeadingH1(markdown: string): string {
  return markdown.replace(/^\s*#\s+[^\n]+\n+/, '');
}

function createUnifiedDocsLoader(): Loader {
  const baseLoader = docsLoader();

  return {
    name: 'thermocam-unified-docs',
    async load(context) {
      await baseLoader.load(context);

      const docsRoot = resolve(process.cwd(), '../docs');

      for (const spec of REPO_DOCS) {
        const absPath = resolve(docsRoot, spec.sourceFile);
        const raw = readFileSync(absPath, 'utf8');
        const body = `${spec.preamble ?? ''}${stripLeadingH1(raw)}`;
        const digest = context.generateDigest(`${spec.title}:${body}`);

        const data = await context.parseData({
          id: spec.id,
          data: {
            title: spec.title,
            description: spec.description,
            editUrl: `https://github.com/maxritter/diy-thermocam/edit/master/docs/${spec.sourceFile}`,
          },
        });

        const rendered = await context.renderMarkdown(body);

        context.store.set({
          id: spec.id,
          data,
          body,
          rendered,
          digest,
        });
      }
    },
  };
}

export const collections = {
  docs: defineCollection({
    loader: createUnifiedDocsLoader(),
    schema: docsSchema(),
  }),
};
