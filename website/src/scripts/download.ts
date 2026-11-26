import { ICONS } from '../lib/icons';
import { DOWNLOADS } from '../lib/site';

/* Studio download: the main button names the visitor's system. */
function detectOS(): 'mac' | 'win' | 'linux' {
  const nav = navigator as Navigator & { userAgentData?: { platform?: string } };
  const p = (
    nav.userAgentData?.platform ||
    navigator.platform ||
    navigator.userAgent ||
    ''
  ).toLowerCase();
  if (p.includes('win')) return 'win';
  if (p.includes('linux') || p.includes('x11')) return 'linux';
  return 'mac';
}
export function paintDownload() {
  const links = document.querySelectorAll<HTMLAnchorElement>('[data-os-download]');
  if (!links.length) return;
  /* Phones and tablets can't run Studio: send them to the list of downloads instead of guessing. */
  if (/android|iphone|ipad|ipod/i.test(navigator.userAgent)) {
    for (const a of links) {
      a.href = '/software/#download';
      a.innerHTML = `${ICONS.download}<span>Studio for your computer</span>`;
    }
    return;
  }
  const os = DOWNLOADS.find((d) => d.os === detectOS()) ?? DOWNLOADS[0];
  for (const a of links) {
    a.href = os.href;
    a.innerHTML = `${ICONS[os.icon]}<span>${os.label}</span>`;
  }
}
