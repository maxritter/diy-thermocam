import { Check, Copy, Download, ExternalLink, Search } from 'lucide-react';
import { useMemo, useState } from 'react';
import type { BomItem, HardwareRevision } from '../lib/bom';

interface Props {
  items: BomItem[];
  csvRaw: string;
}

type EnclosureOption = '3d' | 'laser' | 'none';

const CATEGORY_LABELS: Record<string, string> = {
  all: 'All categories',
  sensor: 'Sensor',
  compute: 'Compute',
  display: 'Display',
  power: 'Power',
  electromechanical: 'Switches',
  connector: 'Connectors',
  pcb_tht: 'PCB (THT)',
  pcb_smd: 'PCB (SMD)',
  storage: 'Storage',
  mechanical: 'PCB & Enclosure',
  fastener: 'Fasteners',
  consumable: 'Cables & Wire',
};

const V3_REUSABLE_IDS = new Set([
  'lepton-3-5',
  'lepton-breakout-v2',
  'teensy-4-1',
  'display-3-2-ili9341',
]);

const ENCLOSURE_IDS = new Set(['enclosure-case', 'tripod-insert-1420']);

function formatUsd(value: number): string {
  return `$${value.toFixed(2)}`;
}

function escapeCsvCell(value: string | number): string {
  const str = String(value);
  if (/[",\r\n]/.test(str)) {
    return `"${str.replace(/"/g, '""')}"`;
  }
  return str;
}

function serializeBomCsv(items: BomItem[]): string {
  const header = [
    'id',
    'hw_rev',
    'category',
    'designators',
    'qty',
    'name',
    'value_spec',
    'package',
    'mpn',
    'manufacturer',
    'lcsc_part',
    'seeed_opl_part',
    'pcba_tier',
    'primary_supplier',
    'primary_url',
    'alt_supplier',
    'alt_url',
    'unit_price_usd',
    'status',
    'notes',
  ];
  const rows = items.map((i) =>
    [
      i.id,
      i.hwRev,
      i.category,
      i.designators,
      i.qty,
      i.name,
      i.valueSpec,
      i.pkg,
      i.mpn,
      i.manufacturer,
      i.lcscPart,
      i.seeedOplPart,
      i.pcbaTier,
      i.primarySupplier,
      i.primaryUrl,
      i.altSupplier,
      i.altUrl,
      i.unitPriceUsd.toFixed(2),
      i.status,
      i.notes,
    ]
      .map(escapeCsvCell)
      .join(','),
  );
  return `${header.join(',')}\n${rows.join('\n')}\n`;
}

export default function BomExplorer({ items, csvRaw }: Props) {
  const [revision, setRevision] = useState<HardwareRevision>('v4.0');
  const [enclosure, setEnclosure] = useState<EnclosureOption>('3d');
  const [includeLepton, setIncludeLepton] = useState<boolean>(true);
  const [includeDisplay, setIncludeDisplay] = useState<boolean>(true);
  const [upgradeFromV3, setUpgradeFromV3] = useState<boolean>(false);
  const [category, setCategory] = useState<string>('all');
  const [query, setQuery] = useState<string>('');
  const [copiedKey, setCopiedKey] = useState<string | null>(null);

  const scopedItems = useMemo(
    () => items.filter((item) => item.hwRev === 'all' || item.hwRev === revision),
    [items, revision],
  );

  const configuredItems = useMemo(() => {
    return scopedItems
      .filter((item) => {
        if (upgradeFromV3 && V3_REUSABLE_IDS.has(item.id)) {
          return false;
        }
        if (
          !includeLepton &&
          (item.id === 'lepton-3-5' || item.id === 'lepton-3-1r' || item.id === 'lepton-2-5')
        ) {
          return false;
        }
        if (!includeDisplay && item.id === 'display-3-2-ili9341') {
          return false;
        }
        if (enclosure === 'none' && ENCLOSURE_IDS.has(item.id)) {
          return false;
        }
        return true;
      })
      .map((item) => {
        if (item.id === 'enclosure-case') {
          if (enclosure === '3d') {
            return {
              ...item,
              valueSpec:
                revision === 'v4.0'
                  ? '3D-Printed PETG / MJF PA12 / CNC Shell (exports/3d-v4.0/)'
                  : '3D-Printed PETG / MJF PA12 / CNC Shell (exports/3d-v3.0/)',
              unitPriceUsd: 10.0,
            };
          }
          if (enclosure === 'laser') {
            return {
              ...item,
              valueSpec:
                revision === 'v4.0'
                  ? '3mm Laser-Cut Cast Acrylic 6-Plate Kit (exports/laser/ v4.0)'
                  : '3mm Laser-Cut Cast Acrylic 6-Plate Kit (exports/laser/ v3.0)',
              unitPriceUsd: 12.0,
            };
          }
        }
        return item;
      });
  }, [scopedItems, upgradeFromV3, includeLepton, includeDisplay, enclosure, revision]);

  const categories = useMemo(() => {
    const present = new Set(configuredItems.map((i) => i.category));
    return ['all', ...Array.from(present)];
  }, [configuredItems]);

  const filteredItems = useMemo(() => {
    const q = query.trim().toLowerCase();
    return configuredItems.filter((item) => {
      if (category !== 'all' && item.category !== category) {
        return false;
      }
      if (!q) {
        return true;
      }
      const haystack = [
        item.name,
        item.valueSpec,
        item.mpn,
        item.manufacturer,
        item.lcscPart,
        item.primarySupplier,
        item.altSupplier,
        item.designators,
        item.notes,
      ]
        .join(' ')
        .toLowerCase();
      return haystack.includes(q);
    });
  }, [configuredItems, category, query]);

  const totals = useMemo(() => {
    const required = configuredItems.filter((item) => item.pcbaTier !== 'optional');
    const fullTotal = required.reduce((sum, item) => sum + item.unitPriceUsd * item.qty, 0);
    const noSensorTotal = required
      .filter((item) => item.id !== 'lepton-3-5')
      .reduce((sum, item) => sum + item.unitPriceUsd * item.qty, 0);
    const lcscCount = required.filter((item) => Boolean(item.lcscPart)).length;
    const basicCount = required.filter((item) => item.pcbaTier === 'basic').length;
    return {
      fullTotal,
      noSensorTotal,
      requiredLines: required.length,
      lcscCount,
      basicCount,
    };
  }, [configuredItems]);

  const isCustomConfig = upgradeFromV3 || !includeLepton || !includeDisplay || enclosure !== '3d';

  const handleCopy = async (text: string, key: string) => {
    try {
      await navigator.clipboard.writeText(text);
      setCopiedKey(key);
      window.setTimeout(() => setCopiedKey(null), 1600);
    } catch {
      // Clipboard API unavailable in restricted context
    }
  };

  const handleDownloadConfiguredCsv = () => {
    const requiredOnly = configuredItems.filter((item) => item.pcbaTier !== 'optional');
    const content = serializeBomCsv(requiredOnly);
    const suffix = upgradeFromV3 ? '-v3-upgrade' : `-${enclosure}`;
    const blob = new Blob([content], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `diy-thermocam-${revision}${suffix}-bom.csv`;
    document.body.appendChild(a);
    a.click();
    a.remove();
    URL.revokeObjectURL(url);
  };

  const handleDownloadMasterCsv = () => {
    const blob = new Blob([csvRaw], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'diy-thermocam-master-bom.csv';
    document.body.appendChild(a);
    a.click();
    a.remove();
    URL.revokeObjectURL(url);
  };

  return (
    <div className="bom-shell not-content" style={{ color: 'var(--tc-text)' }}>
      {/* Revision switch & Cost Summary Bar */}
      <div
        style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fit, minmax(230px, 1fr))',
          gap: '12px',
          marginBottom: '12px',
        }}
      >
        <div
          style={{
            padding: '14px 16px',
            border: '1px solid var(--tc-border)',
            borderRadius: '10px',
            background: 'var(--tc-bg-sunken)',
          }}
        >
          <div style={{ fontSize: '12px', color: 'var(--tc-text-dim)', marginBottom: '8px' }}>
            Hardware revision
          </div>
          <div style={{ display: 'flex', gap: '6px' }}>
            <button
              type="button"
              onClick={() => {
                setRevision('v4.0');
                setCategory('all');
              }}
              aria-pressed={revision === 'v4.0'}
              style={{
                flex: 1,
                padding: '7px 10px',
                borderRadius: '7px',
                fontSize: '13px',
                fontWeight: 600,
                cursor: 'pointer',
                border:
                  revision === 'v4.0'
                    ? '1px solid var(--tc-thermal)'
                    : '1px solid var(--tc-border)',
                background: revision === 'v4.0' ? 'var(--tc-thermal)' : 'var(--tc-surface)',
                color: revision === 'v4.0' ? 'var(--tc-text-on-thermal)' : 'var(--tc-text)',
              }}
            >
              V4.0 Zero-Solder
            </button>
            <button
              type="button"
              onClick={() => {
                setRevision('v3.0');
                setUpgradeFromV3(false);
                setCategory('all');
              }}
              aria-pressed={revision === 'v3.0'}
              style={{
                flex: 1,
                padding: '7px 10px',
                borderRadius: '7px',
                fontSize: '13px',
                fontWeight: 600,
                cursor: 'pointer',
                border:
                  revision === 'v3.0' ? '1px solid var(--tc-accent)' : '1px solid var(--tc-border)',
                background: revision === 'v3.0' ? 'var(--tc-accent)' : 'var(--tc-surface)',
                color: revision === 'v3.0' ? 'var(--tc-text-on-accent)' : 'var(--tc-text)',
              }}
            >
              V3.0 Classic
            </button>
          </div>
        </div>

        <div
          style={{
            padding: '14px 16px',
            border: '1px solid var(--tc-border)',
            borderRadius: '10px',
            background: 'var(--tc-bg-sunken)',
          }}
        >
          <div style={{ fontSize: '12px', color: 'var(--tc-text-dim)' }}>
            {upgradeFromV3
              ? 'Estimated V3 → V4 upgrade total'
              : includeLepton
                ? 'Estimated build total (incl. Lepton 3.5)'
                : 'Estimated build total (excl. Lepton)'}
          </div>
          <div
            className="tabular"
            style={{
              fontSize: '24px',
              fontWeight: 650,
              letterSpacing: '-0.03em',
              marginTop: '2px',
            }}
          >
            {formatUsd(totals.fullTotal)}
          </div>
          <div style={{ fontSize: '12px', color: 'var(--tc-text-muted)', marginTop: '2px' }}>
            {totals.requiredLines} required lines
            {includeLepton && !upgradeFromV3
              ? ` (${formatUsd(totals.noSensorTotal)} excl. Lepton)`
              : ''}
          </div>
        </div>

        <div
          style={{
            padding: '14px 16px',
            border: '1px solid var(--tc-border)',
            borderRadius: '10px',
            background: 'var(--tc-bg-sunken)',
            display: 'flex',
            flexDirection: 'column',
            justifyContent: 'space-between',
            gap: '6px',
          }}
        >
          <div style={{ fontSize: '12px', color: 'var(--tc-text-dim)' }}>
            Single-source CSV (<code>hardware/bom/bom.csv</code>)
          </div>
          <div style={{ display: 'flex', gap: '6px' }}>
            <button
              type="button"
              onClick={handleDownloadConfiguredCsv}
              style={{
                flex: 1,
                display: 'inline-flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '6px',
                padding: '7px 10px',
                borderRadius: '7px',
                border: '1px solid var(--tc-border-strong)',
                background: 'var(--tc-surface)',
                color: 'var(--tc-text)',
                fontSize: '12.5px',
                fontWeight: 600,
                cursor: 'pointer',
              }}
            >
              <Download size={13} />
              {isCustomConfig ? 'Export selection' : `Export ${revision.toUpperCase()} CSV`}
            </button>
            <button
              type="button"
              onClick={handleDownloadMasterCsv}
              title="Download full unmodified hardware/bom/bom.csv"
              style={{
                display: 'inline-flex',
                alignItems: 'center',
                justifyContent: 'center',
                padding: '7px 10px',
                borderRadius: '7px',
                border: '1px solid var(--tc-border)',
                background: 'transparent',
                color: 'var(--tc-text-muted)',
                fontSize: '12px',
                fontWeight: 500,
                cursor: 'pointer',
              }}
            >
              Full CSV
            </button>
          </div>
        </div>
      </div>

      {/* Modular Build Configurator */}
      <div
        style={{
          padding: '12px 16px',
          marginBottom: '12px',
          borderRadius: '10px',
          border: '1px solid var(--tc-border)',
          background: 'var(--tc-surface)',
          display: 'flex',
          flexWrap: 'wrap',
          gap: '16px',
          alignItems: 'center',
          justifyContent: 'space-between',
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', flexWrap: 'wrap' }}>
          <span style={{ fontSize: '12.5px', fontWeight: 600, color: 'var(--tc-text-muted)' }}>
            Enclosure:
          </span>
          {(
            [
              ['3d', '3D Shell'],
              ['laser', '2D Laser Acrylic'],
              ['none', 'None'],
            ] as const
          ).map(([opt, label]) => {
            const active = enclosure === opt;
            return (
              <button
                key={opt}
                type="button"
                onClick={() => setEnclosure(opt)}
                aria-pressed={active}
                style={{
                  padding: '4px 10px',
                  borderRadius: '6px',
                  fontSize: '12.5px',
                  fontWeight: active ? 600 : 500,
                  cursor: 'pointer',
                  border: active ? '1px solid var(--tc-accent-line)' : '1px solid var(--tc-border)',
                  background: active ? 'var(--tc-accent-soft)' : 'var(--tc-bg-sunken)',
                  color: active ? 'var(--tc-accent)' : 'var(--tc-text-muted)',
                }}
              >
                {label}
              </button>
            );
          })}
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '16px', flexWrap: 'wrap' }}>
          <label
            style={{
              display: 'inline-flex',
              alignItems: 'center',
              gap: '6px',
              fontSize: '13px',
              cursor: upgradeFromV3 ? 'not-allowed' : 'pointer',
              opacity: upgradeFromV3 ? 0.55 : 1,
            }}
          >
            <input
              type="checkbox"
              checked={includeLepton && !upgradeFromV3}
              disabled={upgradeFromV3}
              onChange={(e) => setIncludeLepton(e.target.checked)}
            />
            Include FLIR Lepton
          </label>

          <label
            style={{
              display: 'inline-flex',
              alignItems: 'center',
              gap: '6px',
              fontSize: '13px',
              cursor: upgradeFromV3 ? 'not-allowed' : 'pointer',
              opacity: upgradeFromV3 ? 0.55 : 1,
            }}
          >
            <input
              type="checkbox"
              checked={includeDisplay && !upgradeFromV3}
              disabled={upgradeFromV3}
              onChange={(e) => setIncludeDisplay(e.target.checked)}
            />
            Include Display
          </label>

          <label
            style={{
              display: 'inline-flex',
              alignItems: 'center',
              gap: '6px',
              fontSize: '13px',
              fontWeight: 550,
              cursor: 'pointer',
              color: upgradeFromV3 ? 'var(--tc-thermal)' : 'var(--tc-text)',
            }}
          >
            <input
              type="checkbox"
              checked={upgradeFromV3}
              onChange={(e) => {
                const checked = e.target.checked;
                setUpgradeFromV3(checked);
                if (checked && revision !== 'v4.0') {
                  setRevision('v4.0');
                }
              }}
            />
            Upgrading from V3 — reuse Lepton, Breakout, Teensy &amp; Display
          </label>
        </div>
      </div>

      {/* Revision context notice */}
      <div
        style={{
          padding: '10px 14px',
          marginBottom: '14px',
          borderRadius: '8px',
          border:
            revision === 'v4.0'
              ? '1px solid var(--tc-amber-line)'
              : '1px solid var(--tc-pass-line)',
          background: revision === 'v4.0' ? 'var(--tc-amber-soft)' : 'var(--tc-pass-soft)',
          fontSize: '13px',
          lineHeight: 1.5,
          color: 'var(--tc-text)',
        }}
      >
        {revision === 'v3.0' ? (
          <>
            <strong>V3.0 Classic Through-Hole (Proven &amp; buildable):</strong> Verified 1:1 KiCad
            10 port using Teensy 4.1 micro-USB, Pololu U3V16F5 boost, and an external TP4056 charger
            module.
          </>
        ) : (
          <>
            <strong>V4.0 Zero-Solder PCBA (PROTOTYPE-PENDING):</strong> Factory-assembled board with{' '}
            {totals.lcscCount} live-verified LCSC SKUs ({totals.basicCount} JLCPCB Basic) — TI
            BQ24074 USB-C power-path charger, TI TPS61022 5 V boost, AO3401A/AO3400A
            reverse-polarity protection, XC6206 3.3 V RTC LDO (no coin cell), SMD controls, USB pogo
            pins, and slim 505060 LiPo.
          </>
        )}
      </div>

      {/* Filter Toolbar */}
      <div
        style={{
          display: 'flex',
          flexWrap: 'wrap',
          gap: '10px',
          alignItems: 'center',
          marginBottom: '14px',
        }}
      >
        <label
          style={{
            position: 'relative',
            flex: '1 1 240px',
            display: 'flex',
            alignItems: 'center',
          }}
        >
          <Search
            size={15}
            style={{
              position: 'absolute',
              left: '11px',
              color: 'var(--tc-text-dim)',
              pointerEvents: 'none',
            }}
          />
          <input
            type="search"
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            placeholder="Filter by part name, MPN, LCSC code, or supplier..."
            aria-label="Filter BOM items"
            style={{
              width: '100%',
              padding: '8px 12px 8px 34px',
              borderRadius: '8px',
              border: '1px solid var(--tc-border-strong)',
              background: 'var(--tc-surface)',
              color: 'var(--tc-text)',
              fontSize: '13.5px',
            }}
          />
        </label>

        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
          {categories.map((cat) => {
            const active = category === cat;
            return (
              <button
                key={cat}
                type="button"
                onClick={() => setCategory(cat)}
                aria-pressed={active}
                style={{
                  padding: '6px 10px',
                  borderRadius: '7px',
                  fontSize: '12.5px',
                  fontWeight: active ? 600 : 500,
                  cursor: 'pointer',
                  border: active ? '1px solid var(--tc-accent-line)' : '1px solid var(--tc-border)',
                  background: active ? 'var(--tc-accent-soft)' : 'var(--tc-surface)',
                  color: active ? 'var(--tc-accent)' : 'var(--tc-text-muted)',
                }}
              >
                {CATEGORY_LABELS[cat] ?? cat}
              </button>
            );
          })}
        </div>
      </div>

      {/* BOM Data Table */}
      <div
        style={{
          overflowX: 'auto',
          border: '1px solid var(--tc-border)',
          borderRadius: '10px',
          background: 'var(--tc-surface)',
        }}
      >
        <table
          style={{
            width: '100%',
            borderCollapse: 'collapse',
            fontSize: '13px',
            lineHeight: 1.45,
          }}
        >
          <thead>
            <tr
              style={{
                background: 'var(--tc-bg-sunken)',
                borderBottom: '1px solid var(--tc-border)',
              }}
            >
              <th style={{ padding: '10px 12px', textAlign: 'left', fontWeight: 600 }}>
                Part &amp; Spec
              </th>
              <th style={{ padding: '10px 8px', textAlign: 'right', fontWeight: 600 }}>Qty</th>
              <th style={{ padding: '10px 10px', textAlign: 'left', fontWeight: 600 }}>
                MPN / LCSC
              </th>
              <th style={{ padding: '10px 10px', textAlign: 'left', fontWeight: 600 }}>
                Suppliers
              </th>
              <th style={{ padding: '10px 12px', textAlign: 'right', fontWeight: 600 }}>
                Ext. USD
              </th>
            </tr>
          </thead>
          <tbody>
            {filteredItems.map((item) => {
              const extPrice = item.unitPriceUsd * item.qty;
              const isOptional = item.pcbaTier === 'optional';
              return (
                <tr
                  key={item.id}
                  style={{
                    borderBottom: '1px solid var(--tc-border)',
                    opacity: isOptional ? 0.85 : 1,
                  }}
                >
                  <td style={{ padding: '11px 12px', minWidth: '240px' }}>
                    <div
                      style={{
                        display: 'flex',
                        alignItems: 'center',
                        gap: '6px',
                        flexWrap: 'wrap',
                      }}
                    >
                      <strong style={{ color: 'var(--tc-text)', fontWeight: 600 }}>
                        {item.name}
                      </strong>
                      {isOptional && (
                        <span
                          style={{
                            fontSize: '11px',
                            padding: '1px 6px',
                            borderRadius: '5px',
                            border: '1px solid var(--tc-border)',
                            color: 'var(--tc-text-dim)',
                          }}
                        >
                          Optional
                        </span>
                      )}
                      {item.status === 'workaround' && (
                        <span
                          style={{
                            fontSize: '11px',
                            padding: '1px 6px',
                            borderRadius: '5px',
                            border: '1px solid var(--tc-amber-line)',
                            background: 'var(--tc-amber-soft)',
                            color: 'var(--tc-amber)',
                          }}
                        >
                          V3.0 workaround
                        </span>
                      )}
                      {item.status === 'eol_notice' && (
                        <span
                          style={{
                            fontSize: '11px',
                            padding: '1px 6px',
                            borderRadius: '5px',
                            border: '1px solid var(--tc-amber-line)',
                            background: 'var(--tc-amber-soft)',
                            color: 'var(--tc-amber)',
                          }}
                        >
                          EOL 2027
                        </span>
                      )}
                      {item.pcbaTier === 'basic' && (
                        <span
                          style={{
                            fontSize: '11px',
                            padding: '1px 6px',
                            borderRadius: '5px',
                            border: '1px solid var(--tc-pass-line)',
                            background: 'var(--tc-pass-soft)',
                            color: 'var(--tc-pass)',
                          }}
                        >
                          JLCPCB Basic
                        </span>
                      )}
                    </div>
                    <div
                      style={{ fontSize: '12px', color: 'var(--tc-text-muted)', marginTop: '2px' }}
                    >
                      {item.valueSpec}
                      {item.designators ? ` · ${item.designators}` : ''}
                    </div>
                    {item.notes && (
                      <div
                        style={{
                          fontSize: '11.5px',
                          color: 'var(--tc-text-dim)',
                          marginTop: '3px',
                        }}
                      >
                        {item.notes}
                      </div>
                    )}
                  </td>

                  <td
                    className="tabular"
                    style={{
                      padding: '11px 8px',
                      textAlign: 'right',
                      fontFamily: 'var(--tc-font-mono)',
                      fontWeight: 600,
                      whiteSpace: 'nowrap',
                    }}
                  >
                    {item.qty}
                  </td>

                  <td style={{ padding: '11px 10px', whiteSpace: 'nowrap' }}>
                    <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                      {item.mpn && (
                        <button
                          type="button"
                          onClick={() => handleCopy(item.mpn, `${item.id}-mpn`)}
                          title="Copy manufacturer part number"
                          style={{
                            display: 'inline-flex',
                            alignItems: 'center',
                            gap: '5px',
                            fontFamily: 'var(--tc-font-mono)',
                            fontSize: '11.5px',
                            padding: '2px 6px',
                            borderRadius: '5px',
                            border: '1px solid var(--tc-border)',
                            background: 'var(--tc-bg-sunken)',
                            color: 'var(--tc-text)',
                            cursor: 'pointer',
                            width: 'fit-content',
                          }}
                        >
                          {item.mpn}
                          {copiedKey === `${item.id}-mpn` ? (
                            <Check size={11} />
                          ) : (
                            <Copy size={11} />
                          )}
                        </button>
                      )}
                      {item.lcscPart && (
                        <button
                          type="button"
                          onClick={() => handleCopy(item.lcscPart, `${item.id}-lcsc`)}
                          title="Copy LCSC part code"
                          style={{
                            display: 'inline-flex',
                            alignItems: 'center',
                            gap: '5px',
                            fontFamily: 'var(--tc-font-mono)',
                            fontSize: '11.5px',
                            padding: '2px 6px',
                            borderRadius: '5px',
                            border: '1px solid var(--tc-accent-line)',
                            background: 'var(--tc-accent-soft)',
                            color: 'var(--tc-accent)',
                            cursor: 'pointer',
                            width: 'fit-content',
                          }}
                        >
                          LCSC {item.lcscPart}
                          {copiedKey === `${item.id}-lcsc` ? (
                            <Check size={11} />
                          ) : (
                            <Copy size={11} />
                          )}
                        </button>
                      )}
                    </div>
                  </td>

                  <td style={{ padding: '11px 10px', whiteSpace: 'nowrap' }}>
                    <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                      {item.primaryUrl && (
                        <a
                          href={item.primaryUrl}
                          target="_blank"
                          rel="noopener noreferrer"
                          style={{
                            display: 'inline-flex',
                            alignItems: 'center',
                            gap: '4px',
                            color: 'var(--tc-accent)',
                            fontWeight: 500,
                            textDecoration: 'none',
                          }}
                        >
                          {item.primarySupplier}
                          <ExternalLink size={11} />
                        </a>
                      )}
                      {item.altUrl && item.altSupplier && (
                        <a
                          href={item.altUrl}
                          target="_blank"
                          rel="noopener noreferrer"
                          style={{
                            display: 'inline-flex',
                            alignItems: 'center',
                            gap: '4px',
                            color: 'var(--tc-text-muted)',
                            fontSize: '12px',
                            textDecoration: 'none',
                          }}
                        >
                          Alt: {item.altSupplier}
                          <ExternalLink size={11} />
                        </a>
                      )}
                    </div>
                  </td>

                  <td
                    className="tabular"
                    style={{
                      padding: '11px 12px',
                      textAlign: 'right',
                      fontFamily: 'var(--tc-font-mono)',
                      fontWeight: 600,
                      whiteSpace: 'nowrap',
                    }}
                  >
                    {isOptional ? `(${formatUsd(extPrice)})` : formatUsd(extPrice)}
                  </td>
                </tr>
              );
            })}
            {filteredItems.length === 0 && (
              <tr>
                <td
                  colSpan={5}
                  style={{ padding: '24px', textAlign: 'center', color: 'var(--tc-text-dim)' }}
                >
                  No parts match “{query}”. Clear the search box or switch categories to view all
                  components.
                </td>
              </tr>
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
}
