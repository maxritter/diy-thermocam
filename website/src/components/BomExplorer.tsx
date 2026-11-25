import { Check, Copy, Download, ExternalLink, Search } from 'lucide-react';
import { useMemo, useState } from 'react';
import type { BomItem } from '../lib/bom';

interface Props {
  items: BomItem[];
  csvRaw: string;
}

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

function formatUsd(value: number): string {
  return `$${value.toFixed(2)}`;
}

export default function BomExplorer({ items, csvRaw }: Props) {
  const [revision, setRevision] = useState<'v3.0' | 'v3.1'>('v3.0');
  const [category, setCategory] = useState<string>('all');
  const [query, setQuery] = useState<string>('');
  const [copiedKey, setCopiedKey] = useState<string | null>(null);

  const scopedItems = useMemo(
    () => items.filter((item) => item.hwRev === 'all' || item.hwRev === revision),
    [items, revision],
  );

  const categories = useMemo(() => {
    const present = new Set(scopedItems.map((i) => i.category));
    return ['all', ...Array.from(present)];
  }, [scopedItems]);

  const filteredItems = useMemo(() => {
    const q = query.trim().toLowerCase();
    return scopedItems.filter((item) => {
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
  }, [scopedItems, category, query]);

  const totals = useMemo(() => {
    const required = scopedItems.filter((item) => item.pcbaTier !== 'optional');
    const fullTotal = required.reduce((sum, item) => sum + item.unitPriceUsd * item.qty, 0);
    const noSensorTotal = required
      .filter((item) => item.id !== 'lepton-3-5')
      .reduce((sum, item) => sum + item.unitPriceUsd * item.qty, 0);
    const smdCount = required.filter((item) => item.category === 'pcb_smd').length;
    return {
      fullTotal,
      noSensorTotal,
      requiredLines: required.length,
      smdCount,
    };
  }, [scopedItems]);

  const handleCopy = async (text: string, key: string) => {
    try {
      await navigator.clipboard.writeText(text);
      setCopiedKey(key);
      window.setTimeout(() => setCopiedKey(null), 1600);
    } catch {
      // Clipboard API unavailable in restricted context
    }
  };

  const handleDownloadCsv = () => {
    const blob = new Blob([csvRaw], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `diy-thermocam-${revision}-bom.csv`;
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
          gridTemplateColumns: 'repeat(auto-fit, minmax(220px, 1fr))',
          gap: '12px',
          marginBottom: '16px',
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
                setRevision('v3.0');
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
                  revision === 'v3.0'
                    ? '1px solid var(--tc-thermal)'
                    : '1px solid var(--tc-border)',
                background: revision === 'v3.0' ? 'var(--tc-thermal)' : 'var(--tc-surface)',
                color: revision === 'v3.0' ? 'var(--tc-text-on-thermal)' : 'var(--tc-text)',
              }}
            >
              V3.0 THT (Verified)
            </button>
            <button
              type="button"
              onClick={() => {
                setRevision('v3.1');
                setCategory('all');
              }}
              aria-pressed={revision === 'v3.1'}
              style={{
                flex: 1,
                padding: '7px 10px',
                borderRadius: '7px',
                fontSize: '13px',
                fontWeight: 600,
                cursor: 'pointer',
                border:
                  revision === 'v3.1' ? '1px solid var(--tc-accent)' : '1px solid var(--tc-border)',
                background: revision === 'v3.1' ? 'var(--tc-accent)' : 'var(--tc-surface)',
                color: revision === 'v3.1' ? 'var(--tc-text-on-accent)' : 'var(--tc-text)',
              }}
            >
              V3.1 PCBA (USB-C)
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
            Estimated total (with Lepton 3.5)
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
            {totals.requiredLines} required lines ({formatUsd(totals.noSensorTotal)} excl. Lepton)
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
          }}
        >
          <div style={{ fontSize: '12px', color: 'var(--tc-text-dim)' }}>
            Single-source CSV (<code>hardware/bom/bom.csv</code>)
          </div>
          <button
            type="button"
            onClick={handleDownloadCsv}
            style={{
              marginTop: '8px',
              display: 'inline-flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '7px',
              padding: '8px 12px',
              borderRadius: '7px',
              border: '1px solid var(--tc-border-strong)',
              background: 'var(--tc-surface)',
              color: 'var(--tc-text)',
              fontSize: '13px',
              fontWeight: 600,
              cursor: 'pointer',
            }}
          >
            <Download size={14} />
            Download full bom.csv
          </button>
        </div>
      </div>

      {/* Revision context notice */}
      <div
        style={{
          padding: '10px 14px',
          marginBottom: '14px',
          borderRadius: '8px',
          border:
            revision === 'v3.1'
              ? '1px solid var(--tc-amber-line)'
              : '1px solid var(--tc-pass-line)',
          background: revision === 'v3.1' ? 'var(--tc-amber-soft)' : 'var(--tc-pass-soft)',
          fontSize: '13px',
          lineHeight: 1.5,
          color: 'var(--tc-text)',
        }}
      >
        {revision === 'v3.0' ? (
          <>
            <strong>V3.0 Through-Hole (Default):</strong> Proven hand-solderable baseline using
            Teensy 4.1 micro-USB and an external TP4056 charger module.
          </>
        ) : (
          <>
            <strong>V3.1 PCBA + USB-C (Prototype-pending):</strong> Adds {totals.smdCount}{' '}
            single-sided SMD parts (TI BQ24075 power-path charger + board-edge USB-C + ESD
            protection) ready for JLCPCB, Seeed Studio Fusion, or PCBWay PCBA.
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
