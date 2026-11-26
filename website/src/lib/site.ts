export const SITE = {
  name: 'DIY-Thermocam',
  tagline: 'Build your own radiometric thermal camera with a FLIR Lepton 3.5 and a Teensy 4.1',
  url: 'https://www.diy-thermocam.net',
  repoUrl: 'https://github.com/maxritter/diy-thermocam',
  releasesUrl: 'https://github.com/maxritter/diy-thermocam/releases',
  firmwareUrl: 'https://github.com/maxritter/diy-thermocam/releases/latest',
  /* The release workflow uploads the firmware under this fixed name. */
  firmwareFile: 'https://github.com/maxritter/diy-thermocam/releases/latest/download/firmware.hex',
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
  { label: 'Camera', href: '/device-features/' },
  { label: 'Build', href: '/building-instructions/' },
  { label: 'Studio', href: '/software/' },
  { label: 'Gallery', href: '/thermal-images/' },
  { label: 'Docs', href: '/docs/' },
] as const;

/* The launch film, served from this site under a fixed name so a new cut replaces the file and every link
   (README included) keeps working. `seconds` comes from ffprobe on the published file. */
export const FILM = {
  title: 'DIY-Thermocam V4, the film',
  src: '/film/diy-thermocam-v4.mp4',
  captions: '/film/diy-thermocam-v4.vtt',
  poster: '/film/diy-thermocam-v4.jpg',
  seconds: 0,
} as const;

export function filmLength(seconds: number = FILM.seconds): string {
  const s = Math.round(seconds);
  return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`;
}

const DL_BASE = 'https://github.com/maxritter/diy-thermocam/releases/latest/download/';

/* Studio installers. The release workflow has to upload files under these names. */
export const DOWNLOADS = [
  {
    os: 'mac',
    icon: 'apple',
    name: 'macOS',
    label: 'Download for macOS',
    detail: 'Apple silicon and Intel · .dmg',
    href: `${DL_BASE}DIY-Thermocam-Studio-macOS.dmg`,
  },
  {
    os: 'win',
    icon: 'windows',
    name: 'Windows',
    label: 'Download for Windows',
    detail: 'Windows 10 and 11 · .exe installer',
    href: `${DL_BASE}DIY-Thermocam-Studio-Windows.exe`,
  },
  {
    os: 'linux',
    icon: 'linux',
    name: 'Linux',
    label: 'Download for Linux',
    detail: 'x86-64 · .AppImage, also .deb on the release page',
    href: `${DL_BASE}DIY-Thermocam-Studio-Linux.AppImage`,
  },
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
