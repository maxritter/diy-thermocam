import { Download, Film, FolderOpen, HardDrive, RefreshCw, Trash2 } from "lucide-react";
import { useEffect, useRef } from "react";
import { StudioBadge } from "@/components/ui/Primitives";
import { samplePalette } from "@/lib/palettes";
import { generateDemoFrame } from "@/lib/thermal";
import type { Palette, SdListing } from "@/lib/types";

interface SdBrowserPanelProps {
  listing: SdListing | null;
  palette: Palette;
  onRefresh: () => Promise<void>;
  onOpenInEditor: (name: string) => Promise<void>;
  onDownload: (name: string) => Promise<void>;
  onDelete: (name: string) => Promise<void>;
  onExportSequenceMp4: () => Promise<void>;
  statusMsg: string | null;
}

export function SdBrowserPanel({
  listing,
  palette,
  onRefresh,
  onOpenInEditor,
  onDownload,
  onDelete,
  onExportSequenceMp4,
  statusMsg,
}: SdBrowserPanelProps): React.JSX.Element {
  const totalGb = listing ? listing.info.total_kb / (1024 * 1024) : 0;
  const freeGb = listing ? listing.info.free_kb / (1024 * 1024) : 0;
  const usedPct = totalGb > 0 ? Math.min(100, Math.round(((totalGb - freeGb) / totalGb) * 100)) : 0;

  return (
    <div className="flex h-full flex-col gap-3 overflow-y-auto p-3">
      {/* SD Storage Header */}
      <div className="tc-card flex items-center justify-between gap-4 p-4">
        <div className="flex items-center gap-3">
          <div className="flex h-10 w-10 items-center justify-center rounded-lg border border-[var(--tc-border)] bg-[var(--tc-surface-2)]">
            <HardDrive className="h-5 w-5 text-[var(--tc-accent)]" />
          </div>
          <div>
            <div className="flex items-center gap-2">
              <h2 className="text-[14px] font-semibold">Camera microSD Card Browser</h2>
              <StudioBadge tone={listing?.info.card_present ? "pass" : "amber"}>
                {listing?.info.card_present ? "exFAT · Mounted" : "No Card"}
              </StudioBadge>
            </div>
            <div className="mt-1 flex items-center gap-3 text-[12px] text-[var(--tc-text-muted)]">
              <span className="font-mono-tech">
                {freeGb.toFixed(1)} GB free / {totalGb.toFixed(1)} GB ({usedPct}% used)
              </span>
              <div className="h-1.5 w-32 overflow-hidden rounded-full bg-[var(--tc-surface-3)]">
                <div className="h-full bg-[var(--tc-accent)]" style={{ width: `${usedPct}%` }} />
              </div>
              <span className="font-mono-tech">{listing?.entries.length ?? 0} captures</span>
            </div>
          </div>
        </div>

        <div className="flex items-center gap-2">
          <button type="button" onClick={onExportSequenceMp4} className="tc-btn">
            <Film className="h-3.5 w-3.5 text-[var(--tc-accent)]" />
            Sequence → Full HD MP4
          </button>
          <button type="button" onClick={onRefresh} className="tc-btn tc-btn-primary">
            <RefreshCw className="h-3.5 w-3.5" />
            Refresh SD
          </button>
        </div>
      </div>

      {statusMsg ? (
        <div className="rounded-lg border border-[var(--tc-accent-line)] bg-[var(--tc-accent-soft)] px-3 py-2 font-mono-tech text-[11.5px] text-[var(--tc-accent)]">
          {statusMsg}
        </div>
      ) : null}

      {/* Thumbnail Grid */}
      <div className="grid grid-cols-4 gap-3">
        {(listing?.entries ?? []).map((entry, idx) => (
          <div key={entry.name} className="tc-card flex flex-col overflow-hidden">
            <div
              onClick={() => onOpenInEditor(entry.name)}
              className="relative aspect-[4/3] w-full cursor-pointer bg-black"
            >
              <SdThumbCanvas seed={idx * 13 + 3} palette={palette} />
              <span className="absolute right-2 bottom-2 rounded bg-black/75 px-1.5 py-0.5 font-mono-tech text-[10px] text-zinc-200">
                {(entry.size / 1024).toFixed(1)} KB
              </span>
            </div>
            <div className="flex flex-col gap-2 p-2.5">
              <div className="truncate font-mono-tech text-[11.5px] font-medium" title={entry.name}>
                {entry.name}
              </div>
              <div className="flex items-center justify-between gap-1.5">
                <button
                  type="button"
                  onClick={() => onOpenInEditor(entry.name)}
                  className="tc-btn h-7 flex-1 text-[11.5px]"
                >
                  <FolderOpen className="h-3 w-3" />
                  Edit
                </button>
                <button
                  type="button"
                  onClick={() => onDownload(entry.name)}
                  className="tc-btn h-7 px-2"
                  title="Download .DAT to computer"
                >
                  <Download className="h-3.5 w-3.5" />
                </button>
                <button
                  type="button"
                  onClick={() => onDelete(entry.name)}
                  className="tc-btn h-7 px-2 hover:text-[var(--tc-danger)]"
                  title="Delete from camera SD"
                >
                  <Trash2 className="h-3.5 w-3.5" />
                </button>
              </div>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}

function SdThumbCanvas({ seed, palette }: { seed: number; palette: Palette }): React.JSX.Element {
  const ref = useRef<HTMLCanvasElement | null>(null);
  useEffect(() => {
    const cv = ref.current;
    if (!cv) return;
    const ctx = cv.getContext("2d");
    if (!ctx) return;
    const frame = generateDemoFrame(seed, 160, 120);
    const img = ctx.createImageData(160, 120);
    const span = Math.max(1, frame.max_raw - frame.min_raw);
    for (let i = 0; i < frame.pixels.length; i++) {
      const [r, g, b] = samplePalette(palette, (frame.pixels[i] - frame.min_raw) / span);
      img.data[i * 4] = r;
      img.data[i * 4 + 1] = g;
      img.data[i * 4 + 2] = b;
      img.data[i * 4 + 3] = 255;
    }
    ctx.putImageData(img, 0, 0);
  }, [seed, palette]);
  return <canvas ref={ref} width={160} height={120} className="h-full w-full object-cover" />;
}
