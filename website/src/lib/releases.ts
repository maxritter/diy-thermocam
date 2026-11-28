import { SITE } from './site';

export interface FirmwareReleases {
  latest: { version: string; tag: string; url: string };
  legacy: { version: string; tag: string; url: string };
}

export const FALLBACK_RELEASES: FirmwareReleases = {
  latest: { version: '4.0.0', tag: 'v4.0.0', url: `${SITE.releasesUrl}/tag/v4.0.0` },
  legacy: { version: '2.5.0', tag: '2.5.0', url: `${SITE.releasesUrl}/tag/2.5.0` },
};

interface GhRelease {
  tag_name?: string;
  draft?: boolean;
  prerelease?: boolean;
  html_url?: string;
}

function parseSemver(tag: string): [number, number, number] | null {
  const m = /^v?(\d+)\.(\d+)\.(\d+)$/.exec(tag.trim());
  if (!m) return null;
  return [Number(m[1]), Number(m[2]), Number(m[3])];
}

function cmpSemver(a: [number, number, number], b: [number, number, number]): number {
  return a[0] - b[0] || a[1] - b[1] || a[2] - b[2];
}

export function pickReleases(list: GhRelease[]): FirmwareReleases {
  let latest: { v: [number, number, number]; tag: string; url: string } | null = null;
  let legacy: { v: [number, number, number]; tag: string; url: string } | null = null;
  for (const r of list) {
    if (r.draft || r.prerelease || !r.tag_name) continue;
    const v = parseSemver(r.tag_name);
    if (!v) continue;
    const url = r.html_url || `${SITE.releasesUrl}/tag/${r.tag_name}`;
    if (v[0] >= 4 && (!latest || cmpSemver(v, latest.v) > 0)) latest = { v, tag: r.tag_name, url };
    if (v[0] === 2 && (!legacy || cmpSemver(v, legacy.v) > 0)) legacy = { v, tag: r.tag_name, url };
  }
  return {
    latest: latest
      ? { version: latest.v.join('.'), tag: latest.tag, url: latest.url }
      : FALLBACK_RELEASES.latest,
    legacy: legacy
      ? { version: legacy.v.join('.'), tag: legacy.tag, url: legacy.url }
      : FALLBACK_RELEASES.legacy,
  };
}

let cachedPromise: Promise<FirmwareReleases> | null = null;

/** Build-time fetch of GitHub releases so V3/V4 and V1/V2 firmware tags stay current automatically. */
export function getFirmwareReleases(): Promise<FirmwareReleases> {
  if (!cachedPromise) {
    cachedPromise = (async () => {
      try {
        const res = await fetch(
          'https://api.github.com/repos/maxritter/diy-thermocam/releases?per_page=20',
          {
            headers: { Accept: 'application/vnd.github+json', 'User-Agent': 'diy-thermocam-site' },
            signal: AbortSignal.timeout(5000),
          },
        );
        if (!res.ok) return FALLBACK_RELEASES;
        const data = (await res.json()) as unknown;
        if (!Array.isArray(data)) return FALLBACK_RELEASES;
        return pickReleases(data as GhRelease[]);
      } catch {
        return FALLBACK_RELEASES;
      }
    })();
  }
  return cachedPromise;
}
