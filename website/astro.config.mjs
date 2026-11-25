import mdx from '@astrojs/mdx';
import react from '@astrojs/react';
import sitemap from '@astrojs/sitemap';
import starlight from '@astrojs/starlight';
import tailwindcss from '@tailwindcss/vite';
import { defineConfig } from 'astro/config';

export default defineConfig({
  site: 'https://www.diy-thermocam.net',
  trailingSlash: 'always',
  output: 'static',
  integrations: [
    starlight({
      title: 'DIY-Thermocam Docs',
      description:
        'Technical documentation, interactive BOM, Teensy 4.1 firmware guide, USB Protocol v2 spec, and desktop app reference for DIY-Thermocam V3.',
      logo: {
        light: './public/images/brand/logo-light.svg',
        dark: './public/images/brand/logo-dark.svg',
        replacesTitle: true,
      },
      favicon: '/favicon.svg',
      social: [
        {
          icon: 'github',
          label: 'GitHub',
          href: 'https://github.com/maxritter/diy-thermocam',
        },
        {
          icon: 'youtube',
          label: 'YouTube',
          href: 'https://www.youtube.com/channel/UC4XjTWGf-Umvk3ypZHzNSAw',
        },
      ],
      editLink: {
        baseUrl: 'https://github.com/maxritter/diy-thermocam/edit/master/',
      },
      customCss: ['./src/styles/starlight.css'],
      pagefind: true,
      head: [
        {
          tag: 'script',
          attrs: {
            defer: true,
            src: 'https://cloud.umami.is/script.js',
            'data-website-id': 'efb7f48a-b0e1-46ff-9e74-2a2c1a76e868',
          },
        },
        {
          tag: 'meta',
          attrs: {
            property: 'og:image',
            content: 'https://www.diy-thermocam.net/images/og/og-default.png',
          },
        },
        {
          tag: 'meta',
          attrs: {
            name: 'twitter:card',
            content: 'summary_large_image',
          },
        },
        {
          tag: 'meta',
          attrs: {
            name: 'twitter:image',
            content: 'https://www.diy-thermocam.net/images/og/og-default.png',
          },
        },
        {
          tag: 'link',
          attrs: {
            rel: 'icon',
            type: 'image/png',
            href: '/images/website/icon.png',
          },
        },
      ],
      sidebar: [
        {
          label: 'Get Started',
          items: [
            { label: 'Documentation Home', slug: 'docs' },
            { label: 'User Manual', slug: 'docs/manual' },
            { label: 'Building Instructions (45 Steps)', link: '/building-instructions/' },
          ],
        },
        {
          label: 'Hardware & Fabrication',
          items: [
            { label: 'Bill of Materials (Partlist)', slug: 'docs/partlist' },
            { label: 'Hardware, PCB & Enclosure', slug: 'docs/hardware' },
          ],
        },
        {
          label: 'Firmware & Protocol',
          items: [
            { label: 'Firmware & Flashing', slug: 'docs/firmware' },
            { label: 'USB Protocol v2 Spec', slug: 'docs/protocol-v2' },
            { label: 'USB Serial Protocol', slug: 'docs/serial' },
            { label: 'Raw Data Format (.DAT)', slug: 'docs/raw-data' },
          ],
        },
        {
          label: 'Desktop App & Site',
          items: [
            { label: 'Desktop App Reference', slug: 'docs/desktop-app' },
            { label: 'Back to Main Website', link: '/' },
          ],
        },
      ],
    }),
    react(),
    mdx(),
    sitemap(),
  ],
  vite: {
    plugins: [tailwindcss()],
    server: {
      fs: {
        allow: ['..'],
      },
    },
  },
});
