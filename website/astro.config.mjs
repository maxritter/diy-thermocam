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
        'Build, use and extend DIY-Thermocam: parts list, build guide, Studio, firmware updates, USB protocol and the .DAT file format.',
      components: {
        SiteTitle: './src/components/starlight/SiteTitle.astro',
        ThemeSelect: './src/components/starlight/ThemeSelect.astro',
        Footer: './src/components/starlight/Footer.astro',
      },
      favicon: '/favicon.svg',
      social: [
        {
          icon: 'github',
          label: 'DIY-Thermocam on GitHub',
          href: 'https://github.com/maxritter/diy-thermocam',
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
            href: '/images/brand/favicon.png',
          },
        },
      ],
      sidebar: [
        {
          label: 'Get started',
          items: [
            { label: 'Overview', slug: 'docs' },
            { label: 'Parts list', slug: 'docs/partlist' },
            { label: 'Build guide', link: '/building-instructions/' },
            { label: 'Install Studio', link: '/software/#download' },
          ],
        },
        {
          label: 'Use the camera',
          items: [
            { label: 'Camera controls', slug: 'docs/manual' },
            { label: 'Studio', slug: 'docs/desktop-app' },
            { label: 'Firmware updates', slug: 'docs/firmware' },
          ],
        },
        {
          label: 'Reference',
          items: [
            { label: 'Board and shell', slug: 'docs/hardware' },
            { label: 'USB protocol', slug: 'docs/protocol-v2' },
            { label: '.DAT file format', slug: 'docs/raw-data' },
          ],
        },
      ],
    }),
    react(),
    mdx(),
    /* /docs/serial/ repeats /docs/protocol-v2/ for old links; only the original belongs in the sitemap. */
    sitemap({ filter: (page) => !page.endsWith('/docs/serial/') }),
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
