/**
 * Capture names (captures.html#library): the camera and Studio both name a capture
 * `YYYYMMDDHHMMSS.DAT` (a recording's folder the same way, `-2`, `-3` on a collision), shown as
 * "25 Nov 2026 · 10:20:14". A name typed in Notes wins; anything else is kept as it is.
 */

export interface ParsedName {
  /** Local time the name encodes, or null when it is not a real date and time. */
  taken: Date | null;
  /** The `-N` collision suffix, if any. */
  suffix: number | null;
}

const NAME_RE = /^(\d{4})(\d{2})(\d{2})(\d{2})(\d{2})(\d{2})(?:-(\d+))?(?:\.dat)?$/i;

export function parseCaptureName(fileName: string): ParsedName {
  const m = NAME_RE.exec(fileName);
  if (!m) return { taken: null, suffix: null };
  const [y, mo, d, h, mi, s] = m.slice(1, 7).map(Number);
  const suffix = m[7] ? Number(m[7]) : null;
  if (mo < 1 || mo > 12 || d < 1 || h > 23 || mi > 59 || s > 59) {
    return { taken: null, suffix: null };
  }
  const taken = new Date(y, mo - 1, d, h, mi, s);
  // Day 31 of a 30-day month rolls over into the next month: not a real date.
  if (taken.getFullYear() !== y || taken.getMonth() !== mo - 1 || taken.getDate() !== d) {
    return { taken: null, suffix: null };
  }
  return { taken, suffix };
}

export function systemLocale(): string {
  try {
    return Intl.DateTimeFormat().resolvedOptions().locale;
  } catch {
    return "en-GB";
  }
}

const pad = (n: number) => String(n).padStart(2, "0");

function monthShort(d: Date, locale: string): string {
  return new Intl.DateTimeFormat(locale, { month: "short" }).format(d).replace(/\.$/, "");
}

function monthLong(d: Date, locale: string): string {
  return new Intl.DateTimeFormat(locale, { month: "long" }).format(d);
}

function weekday(d: Date, locale: string): string {
  return new Intl.DateTimeFormat(locale, { weekday: "long" }).format(d);
}

/** "25 Nov 2026 · 10:20:14", with the month name in the system locale. */
export function formatTaken(d: Date, locale: string = systemLocale()): string {
  return `${d.getDate()} ${monthShort(d, locale)} ${d.getFullYear()} · ${pad(d.getHours())}:${pad(
    d.getMinutes(),
  )}:${pad(d.getSeconds())}`;
}

/** The name a capture shows before anyone types one: its date and time, else the file name. */
export function captureLabel(fileName: string, locale: string = systemLocale()): string {
  const { taken, suffix } = parseCaptureName(fileName);
  if (!taken) return fileName;
  return suffix ? `${formatTaken(taken, locale)} (${suffix})` : formatTaken(taken, locale);
}

/**
 * A card's two lines. A typed name replaces the date-time title and the date and time (or the
 * file name) move to the line below, before the size or frame count.
 */
export function cardText(
  fileName: string,
  typedName: string | null | undefined,
  detail: string,
  locale: string = systemLocale(),
): { title: string; meta: string } {
  const label = captureLabel(fileName, locale);
  const typed = typedName?.trim();
  if (!typed) return { title: label, meta: detail };
  return { title: typed, meta: detail ? `${label} · ${detail}` : label };
}

function startOfDay(d: Date): number {
  return new Date(d.getFullYear(), d.getMonth(), d.getDate()).getTime();
}

/** Whole days from `d` to `now` by the calendar (0 today, 1 yesterday). */
function daysAgo(d: Date, now: Date): number {
  return Math.round((startOfDay(now) - startOfDay(d)) / 86_400_000);
}

/** A day's heading: "Today · Wednesday, 25 November", "Monday · 23 November", older with the year. */
export function dayHeading(
  d: Date,
  now: Date,
  locale: string = systemLocale(),
): { title: string; sub: string } {
  const ago = daysAgo(d, now);
  const dayMonth = `${d.getDate()} ${monthLong(d, locale)}`;
  if (ago === 0) return { title: "Today", sub: `${weekday(d, locale)}, ${dayMonth}` };
  if (ago === 1) return { title: "Yesterday", sub: `${weekday(d, locale)}, ${dayMonth}` };
  if (ago > 1 && ago < 7) return { title: weekday(d, locale), sub: dayMonth };
  return { title: `${dayMonth} ${d.getFullYear()}`, sub: weekday(d, locale) };
}

export interface DayGroup<T> {
  key: string;
  title: string;
  sub: string;
  items: T[];
}

/**
 * Groups by calendar day in the order given (the sort decides), then "No date" for names that
 * are not a date and time, then "Other files" for what is not a capture.
 */
export function groupByDay<T extends { taken: Date | null; other: boolean }>(
  items: T[],
  now: Date,
  locale: string = systemLocale(),
): DayGroup<T>[] {
  const days = new Map<string, DayGroup<T>>();
  const undated: T[] = [];
  const other: T[] = [];
  for (const item of items) {
    if (item.other) {
      other.push(item);
      continue;
    }
    if (!item.taken) {
      undated.push(item);
      continue;
    }
    const key = String(startOfDay(item.taken));
    let g = days.get(key);
    if (!g) {
      g = { key, ...dayHeading(item.taken, now, locale), items: [] };
      days.set(key, g);
    }
    g.items.push(item);
  }
  const out = [...days.values()];
  if (undated.length) out.push({ key: "no-date", title: "No date", sub: "", items: undated });
  if (other.length) out.push({ key: "other", title: "Other files", sub: "", items: other });
  return out;
}

/** "38 KB" (a 38,805-byte `.DAT`), "4.4 MB" for a card's line, in 1024s as the card counts. */
export function formatSize(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 ** 2) return `${Math.round(bytes / 1024)} KB`;
  if (bytes < 1024 ** 3) return `${(bytes / 1024 ** 2).toFixed(1)} MB`;
  return `${(bytes / 1024 ** 3).toFixed(1)} GB`;
}
