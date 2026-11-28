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
  /* For a page that repeats another, point search engines at the original. */
  canonical?: string;
}

const REPO_DOCS: RepoDocSpec[] = [
  {
    id: 'docs/manual',
    sourceFile: 'manual.md',
    title: 'Camera controls',
    description: 'Buttons, touch menu, palettes, charging and saving captures on DIY-Thermocam.',
  },
  {
    id: 'docs/hardware',
    sourceFile: 'hardware.md',
    title: 'Board and shell',
    description:
      'The DIY-Thermocam V4 main board, pin map, fab options, and the printed and laser-cut shells.',
  },
  {
    id: 'docs/firmware',
    sourceFile: 'firmware.md',
    title: 'Firmware updates',
    description:
      'Update the DIY-Thermocam firmware with Studio, or build it from source with arduino-cli.',
  },
  {
    id: 'docs/raw-data',
    sourceFile: 'raw-data-format.md',
    title: '.DAT file format',
    description:
      'How a radiometric DIY-Thermocam capture is stored, and how to turn raw values into temperatures.',
  },
  {
    id: 'docs/protocol-v2',
    sourceFile: 'protocol-v2.md',
    title: 'USB protocol',
    description:
      'Packet format and commands for talking to DIY-Thermocam over USB from your own software.',
  },
  {
    id: 'docs/serial',
    sourceFile: 'protocol-v2.md',
    title: 'USB protocol (serial)',
    canonical: 'https://www.diy-thermocam.net/docs/protocol-v2/',
    description: "DIY-Thermocam's USB protocol, kept at this address for older links.",
    preamble:
      '> This page keeps its address for older links. It shows the same content as [USB protocol](/docs/protocol-v2/).\n\n',
  },
  {
    id: 'docs/desktop-app',
    sourceFile: 'software.md',
    title: 'Studio',
    description:
      'DIY-Thermocam Studio for macOS, Windows and Linux: live view, measuring, SD card and exports.',
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
            ...(spec.canonical
              ? { head: [{ tag: 'link', attrs: { rel: 'canonical', href: spec.canonical } }] }
              : {}),
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
