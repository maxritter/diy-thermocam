export const SITE = {
  name: 'DIY-Thermocam',
  tagline: 'Open-source radiometric thermal imaging built on FLIR Lepton and Teensy 4.1',
  url: 'https://www.diy-thermocam.net',
  repoUrl: 'https://github.com/maxritter/diy-thermocam',
  releasesUrl: 'https://github.com/maxritter/diy-thermocam/releases',
  ogImage: 'https://www.diy-thermocam.net/images/og/og-default.png',
  umamiWebsiteId: 'efb7f48a-b0e1-46ff-9e74-2a2c1a76e868',
  author: {
    name: 'Max Ritter',
    website: 'https://maxritter.net/',
    github: 'https://github.com/maxritter',
    youtube: 'https://www.youtube.com/channel/UC4XjTWGf-Umvk3ypZHzNSAw',
    linkedin: 'https://www.linkedin.com/in/rittermax/',
  },
} as const;

export const NAV_LINKS = [
  { label: 'Build', href: '/building-instructions/' },
  { label: 'Features', href: '/device-features/' },
  { label: 'Software', href: '/software/' },
  { label: 'Thermal', href: '/thermal-images/' },
  { label: 'Hardware', href: '/device-images/' },
  { label: 'Docs', href: '/docs/' },
] as const;

export function buildBreadcrumbs(items: Array<{ name: string; path: string }>) {
  return {
    '@context': 'https://schema.org',
    '@type': 'BreadcrumbList',
    itemListElement: items.map((item, idx) => ({
      '@type': 'ListItem',
      position: idx + 1,
      name: item.name,
      item: `${SITE.url}${item.path}`,
    })),
  };
}
