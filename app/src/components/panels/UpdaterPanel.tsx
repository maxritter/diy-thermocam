import { CheckCircle2, Cpu, FileUp, ShieldCheck, UploadCloud } from "lucide-react";
import { useEffect, useState } from "react";
import { StudioBadge, StudioSwitch } from "@/components/ui/Primitives";
import { backend } from "@/lib/backend";
import type { FirmwareImageInfo } from "@/lib/types";

const SAMPLE_TEENSY41_HEX = `:0200000460009A
:100000004643464200000156000000000000000088
:100010001112131415161718191A1B1C1D1E1F2058
:00000001FF`;

interface UpdaterPanelProps {
  connectedPort: string;
}

export function UpdaterPanel({ connectedPort }: UpdaterPanelProps): React.JSX.Element {
  const [hexText, setHexText] = useState(SAMPLE_TEENSY41_HEX);
  const [expectedSha, setExpectedSha] = useState("");
  const [useLegacy134, setUseLegacy134] = useState(false);
  const [verified, setVerified] = useState<FirmwareImageInfo | null>(null);
  const [progress, setProgress] = useState(0);
  const [status, setStatus] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [flashing, setFlashing] = useState(false);

  useEffect(() => {
    let active = true;
    let unlisten: (() => void) | null = null;
    void backend
      .onFlashProgress((ev) => {
        if (!active) return;
        if (ev.stage === "verified") {
          setProgress(10);
          setStatus("Firmware HEX verified; triggering HalfKay bootloader…");
        } else if (ev.stage === "waiting_bootloader") {
          setProgress(20);
          setStatus("Waiting up to 10 s for HalfKay USB HID (16C0:0478) enumeration…");
        } else if (ev.stage === "flashing" && ev.done && ev.total) {
          const pct = Math.min(98, 20 + Math.round((ev.done / ev.total) * 78));
          setProgress(pct);
          setStatus(`Flashing Teensy 4.1 block ${ev.done}/${ev.total}…`);
        } else if (ev.stage === "rebooted") {
          setProgress(100);
        }
      })
      .then((fn) => {
        if (active) unlisten = fn;
        else fn();
      });
    return () => {
      active = false;
      unlisten?.();
    };
  }, []);

  const handleVerify = async (): Promise<void> => {
    try {
      setError(null);
      const info = await backend.verifyHex(hexText, expectedSha.trim() || undefined);
      setVerified(info);
      setStatus(
        `Verified Intel HEX (${info.total_bytes} B across ${info.block_count} Teensy 4.1 1024-B flash blocks)`,
      );
    } catch (err) {
      setVerified(null);
      setError(err instanceof Error ? err.message : String(err));
    }
  };

  const handleFlash = async (): Promise<void> => {
    setFlashing(true);
    setError(null);
    setProgress(15);
    try {
      const sha = await backend.flashFirmware(
        hexText,
        expectedSha.trim() || undefined,
        useLegacy134 ? connectedPort : undefined,
      );
      setProgress(100);
      setStatus(`HalfKay HID flash complete & rebooted · SHA-256 ${sha.slice(0, 16)}…`);
    } catch (err) {
      setProgress(0);
      setError(err instanceof Error ? err.message : String(err));
    } finally {
      setFlashing(false);
    }
  };

  return (
    <div className="grid h-full grid-cols-12 gap-3 overflow-y-auto p-3">
      <div className="tc-card col-span-7 space-y-3.5 p-4">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-2">
            <Cpu className="h-4 w-4 text-[var(--tc-accent)]" />
            <h2 className="text-[14px] font-semibold">Native Teensy 4.1 HalfKay HID Updater</h2>
          </div>
          <StudioBadge tone="pass">Zero External CLI</StudioBadge>
        </div>

        <p className="text-[12px] text-[var(--tc-text-muted)]">
          Parses Intel HEX, verifies SHA-256, enters HalfKay bootloader (`16C0:0478`) via Protocol
          v2 `ENTER_BOOTLOADER` (or legacy 134-baud CDC reset), and streams 1088-byte HID flash
          packets natively in Rust.
        </p>

        <div>
          <div className="mb-1 flex items-center justify-between">
            <label className="text-[11.5px] text-[var(--tc-text-muted)]">
              Intel HEX Firmware Image
            </label>
            <button
              type="button"
              onClick={() => setHexText(SAMPLE_TEENSY41_HEX)}
              className="text-[11px] text-[var(--tc-accent)] hover:underline cursor-pointer"
            >
              Load Sample Teensy 4.1 Test HEX
            </button>
          </div>
          <textarea
            value={hexText}
            onChange={(e) => {
              setHexText(e.target.value);
              setVerified(null);
            }}
            rows={6}
            className="w-full rounded-lg border border-[var(--tc-border)] bg-[var(--tc-surface-2)] p-2.5 font-mono-tech text-[11.5px]"
          />
        </div>

        <div className="grid grid-cols-2 gap-3">
          <div>
            <label className="mb-1 block text-[11.5px] text-[var(--tc-text-muted)]">
              Optional Expected SHA-256 Digest
            </label>
            <input
              value={expectedSha}
              onChange={(e) => setExpectedSha(e.target.value)}
              placeholder="Paste release SHA-256 to enforce match"
              className="tc-input w-full font-mono-tech text-[11.5px]"
            />
          </div>
          <div className="flex items-end pb-1">
            <StudioSwitch
              checked={useLegacy134}
              onCheckedChange={setUseLegacy134}
              label="Legacy v3.0.x 134-baud CDC fallback"
            />
          </div>
        </div>

        <div className="flex items-center gap-2 pt-1">
          <button type="button" onClick={handleVerify} className="tc-btn">
            <ShieldCheck className="h-3.5 w-3.5 text-[var(--tc-pass)]" />
            Verify Intel HEX &amp; SHA-256
          </button>
          <button
            type="button"
            disabled={flashing || !hexText.trim()}
            onClick={handleFlash}
            className="tc-btn tc-btn-primary"
          >
            <UploadCloud className="h-3.5 w-3.5" />
            {flashing ? "Flashing HalfKay HID…" : "Enter Bootloader & Flash Firmware"}
          </button>
        </div>

        {progress > 0 ? (
          <div className="space-y-1">
            <div className="flex justify-between font-mono-tech text-[11px]">
              <span>HalfKay HID 1088-B Packet Transfer</span>
              <span>{progress}%</span>
            </div>
            <div className="h-2 w-full overflow-hidden rounded-full bg-[var(--tc-surface-2)]">
              <div
                className="h-full bg-[var(--tc-pass)] transition-all"
                style={{ width: `${progress}%` }}
              />
            </div>
          </div>
        ) : null}

        {status ? (
          <div className="flex items-center gap-2 rounded-lg border border-[var(--tc-pass-line)] bg-[var(--tc-pass-soft)] px-3 py-2 font-mono-tech text-[11.5px] text-[var(--tc-pass)]">
            <CheckCircle2 className="h-4 w-4 shrink-0" />
            <span>{status}</span>
          </div>
        ) : null}

        {error ? (
          <div className="rounded-lg border border-[var(--tc-danger-line)] bg-[var(--tc-danger-soft)] px-3 py-2 font-mono-tech text-[11.5px] text-[var(--tc-danger)]">
            {error}
          </div>
        ) : null}
      </div>

      {/* Right: Verification Summary & Bootloader Protocol Checklist */}
      <div className="tc-card col-span-5 space-y-3 p-4">
        <div className="flex items-center gap-2">
          <FileUp className="h-4 w-4 text-[var(--tc-accent)]" />
          <h3 className="text-[13.5px] font-semibold">Firmware Package Verification</h3>
        </div>

        <div className="rounded-lg border border-[var(--tc-border)] bg-[var(--tc-surface-2)] p-3 font-mono-tech text-[11.5px] space-y-2">
          <div className="flex justify-between">
            <span className="text-[var(--tc-text-muted)]">Target MCU</span>
            <span>Teensy 4.1 (i.MX RT1062)</span>
          </div>
          <div className="flex justify-between">
            <span className="text-[var(--tc-text-muted)]">HID VID:PID</span>
            <span>16C0:0478 (HalfKay)</span>
          </div>
          <div className="flex justify-between">
            <span className="text-[var(--tc-text-muted)]">Block / Report Size</span>
            <span>1024 B / 1088 B</span>
          </div>
          <div className="flex justify-between">
            <span className="text-[var(--tc-text-muted)]">Payload Bytes</span>
            <span>{verified ? `${verified.total_bytes} B` : "Not verified yet"}</span>
          </div>
          <div className="flex justify-between">
            <span className="text-[var(--tc-text-muted)]">1024-B Flash Blocks</span>
            <span>{verified ? verified.block_count : "—"}</span>
          </div>
          <div>
            <span className="block text-[var(--tc-text-muted)]">SHA-256 Digest</span>
            <span className="mt-0.5 block break-all text-[10.5px] text-[var(--tc-accent)]">
              {verified ? verified.sha256_hex : "Click Verify to compute SHA-256"}
            </span>
          </div>
        </div>

        <div className="rounded-lg border border-[var(--tc-amber-line)] bg-[var(--tc-amber-soft)] p-3 text-[12px] text-[var(--tc-amber-ink)]">
          Safety note: Updating firmware stops active streaming, sends `ENTER_BOOTLOADER`
          (`0xB00710AD`), writes verified 1024-byte blocks to FlexSPI NOR, and issues the HalfKay
          reboot command (`0xFF 0xFF 0xFF`).
        </div>
      </div>
    </div>
  );
}
