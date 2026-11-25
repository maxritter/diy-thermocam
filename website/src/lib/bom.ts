import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

export type HardwareRevision = 'v3.0' | 'v4.0';

export interface BomItem {
  id: string;
  hwRev: 'all' | HardwareRevision;
  category: string;
  designators: string;
  qty: number;
  name: string;
  valueSpec: string;
  pkg: string;
  mpn: string;
  manufacturer: string;
  lcscPart: string;
  seeedOplPart: string;
  pcbaTier: string;
  primarySupplier: string;
  primaryUrl: string;
  altSupplier: string;
  altUrl: string;
  unitPriceUsd: number;
  status: string;
  notes: string;
}

function parseCsvLine(line: string): string[] {
  const fields: string[] = [];
  let current = '';
  let inQuotes = false;

  for (let i = 0; i < line.length; i += 1) {
    const ch = line[i];
    if (inQuotes) {
      if (ch === '"' && line[i + 1] === '"') {
        current += '"';
        i += 1;
      } else if (ch === '"') {
        inQuotes = false;
      } else {
        current += ch;
      }
    } else if (ch === '"') {
      inQuotes = true;
    } else if (ch === ',') {
      fields.push(current);
      current = '';
    } else {
      current += ch;
    }
  }
  fields.push(current);
  return fields;
}

export function loadBomCsvRaw(): string {
  const csvPath = resolve(process.cwd(), '../hardware/bom/bom.csv');
  return readFileSync(csvPath, 'utf8');
}

export function loadBomItems(): BomItem[] {
  const raw = loadBomCsvRaw();
  const lines = raw
    .split(/\r?\n/)
    .map((l) => l.trim())
    .filter((l) => l.length > 0);

  if (lines.length < 2) {
    return [];
  }

  return lines.slice(1).map((line) => {
    const cols = parseCsvLine(line);
    const hwRevRaw = (cols[1] || 'all').trim().toLowerCase();
    const hwRev: BomItem['hwRev'] =
      hwRevRaw === 'v3.0' ? 'v3.0' : hwRevRaw === 'v4.0' ? 'v4.0' : 'all';

    return {
      id: cols[0] ?? '',
      hwRev,
      category: cols[2] ?? '',
      designators: cols[3] ?? '',
      qty: Number.parseInt(cols[4] ?? '1', 10) || 1,
      name: cols[5] ?? '',
      valueSpec: cols[6] ?? '',
      pkg: cols[7] ?? '',
      mpn: cols[8] ?? '',
      manufacturer: cols[9] ?? '',
      lcscPart: cols[10] ?? '',
      seeedOplPart: cols[11] ?? '',
      pcbaTier: cols[12] ?? '',
      primarySupplier: cols[13] ?? '',
      primaryUrl: cols[14] ?? '',
      altSupplier: cols[15] ?? '',
      altUrl: cols[16] ?? '',
      unitPriceUsd: Number.parseFloat(cols[17] ?? '0') || 0,
      status: cols[18] ?? 'active',
      notes: cols[19] ?? '',
    };
  });
}

export function computeBomTotals(items: BomItem[], revision: HardwareRevision) {
  const scoped = items.filter((item) => item.hwRev === 'all' || item.hwRev === revision);
  const required = scoped.filter((item) => item.pcbaTier !== 'optional');
  const optional = scoped.filter((item) => item.pcbaTier === 'optional');
  const requiredUsd = required.reduce((sum, item) => sum + item.unitPriceUsd * item.qty, 0);
  const withoutSensorUsd = required
    .filter((item) => item.id !== 'lepton-3-5')
    .reduce((sum, item) => sum + item.unitPriceUsd * item.qty, 0);
  return {
    totalRows: scoped.length,
    requiredRows: required.length,
    optionalRows: optional.length,
    requiredUsd: Number(requiredUsd.toFixed(2)),
    withoutSensorUsd: Number(withoutSensorUsd.toFixed(2)),
  };
}
