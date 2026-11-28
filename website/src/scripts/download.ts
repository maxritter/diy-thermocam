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

interface ReleaseSummary {
  latest: { version: string; tag: string; url: string };
  legacy: { version: string; tag: string; url: string };
}

const RELEASE_CACHE_KEY = 'diy-thermocam-releases-v1';
const RELEASE_CACHE_TTL_MS = 10 * 60 * 1000;

function parseReleases(
  list: Array<{ tag_name?: string; draft?: boolean; prerelease?: boolean; html_url?: string }>,
): ReleaseSummary | null {
  const parse = (t: string) => {
    const m = /^v?(\d+)\.(\d+)\.(\d+)$/.exec(t.trim());
    return m ? ([Number(m[1]), Number(m[2]), Number(m[3])] as const) : null;
  };
  const cmp = (a: readonly [number, number, number], b: readonly [number, number, number]) =>
    a[0] - b[0] || a[1] - b[1] || a[2] - b[2];
  let latest: { v: readonly [number, number, number]; tag: string; url: string } | null = null;
  let legacy: { v: readonly [number, number, number]; tag: string; url: string } | null = null;
  for (const r of list) {
    if (r.draft || r.prerelease || !r.tag_name) continue;
    const v = parse(r.tag_name);
    if (!v) continue;
    const url =
      r.html_url || `https://github.com/maxritter/diy-thermocam/releases/tag/${r.tag_name}`;
    if (v[0] >= 4 && (!latest || cmp(v, latest.v) > 0)) latest = { v, tag: r.tag_name, url };
    if (v[0] === 2 && (!legacy || cmp(v, legacy.v) > 0)) legacy = { v, tag: r.tag_name, url };
  }
  if (!latest && !legacy) return null;
  return {
    latest: latest
      ? { version: latest.v.join('.'), tag: latest.tag, url: latest.url }
      : {
          version: '4.0.0',
          tag: 'v4.0.0',
          url: 'https://github.com/maxritter/diy-thermocam/releases/tag/v4.0.0',
        },
    legacy: legacy
      ? { version: legacy.v.join('.'), tag: legacy.tag, url: legacy.url }
      : {
          version: '2.5.0',
          tag: '2.5.0',
          url: 'https://github.com/maxritter/diy-thermocam/releases/tag/2.5.0',
        },
  };
}

function applyReleases(rel: ReleaseSummary) {
  for (const el of document.querySelectorAll<HTMLElement>('[data-fw-latest-ver]')) {
    el.textContent = rel.latest.version;
  }
  for (const el of document.querySelectorAll<HTMLElement>('[data-fw-legacy-ver]')) {
    el.textContent = rel.legacy.version;
  }
  for (const a of document.querySelectorAll<HTMLAnchorElement>('a[data-fw-legacy-link]')) {
    a.href = rel.legacy.url;
  }
}

/** Keep V3/V4 and V1/V2 firmware tags fresh from GitHub Releases even between static deploys. */
export function refreshReleases() {
  if (!document.querySelector('[data-fw-legacy-ver], [data-fw-latest-ver]')) return;
  try {
    const raw = sessionStorage.getItem(RELEASE_CACHE_KEY);
    if (raw) {
      const parsed = JSON.parse(raw) as { at: number; rel: ReleaseSummary };
      if (Date.now() - parsed.at < RELEASE_CACHE_TTL_MS && parsed.rel) {
        applyReleases(parsed.rel);
        return;
      }
    }
  } catch {
    /* ignore storage errors */
  }
  fetch('https://api.github.com/repos/maxritter/diy-thermocam/releases?per_page=15', {
    headers: { Accept: 'application/vnd.github+json' },
  })
    .then((r) => (r.ok ? r.json() : null))
    .then((data) => {
      if (!Array.isArray(data)) return;
      const rel = parseReleases(data);
      if (!rel) return;
      applyReleases(rel);
      try {
        sessionStorage.setItem(RELEASE_CACHE_KEY, JSON.stringify({ at: Date.now(), rel }));
      } catch {
        /* ignore storage errors */
      }
    })
    .catch(() => {
      /* offline or rate-limited: keep build-time values */
    });
}
