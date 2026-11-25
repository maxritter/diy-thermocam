import { useCurrentFrame, useVideoConfig } from 'remotion'
import type { NarrationCaption } from '../captions'
import { C, FONT } from '../theme'

export function NarrationCaptions({ captions }: { captions: NarrationCaption[] }) {
  const frame = useCurrentFrame()
  const { fps } = useVideoConfig()
  const timeMs = (frame / fps) * 1000
  const caption = captions.find((p) => timeMs >= p.startMs && timeMs < p.endMs)
  if (!caption) return null
  return (
    <div
      data-narration-caption={caption.chunkId}
      style={{
        position: 'absolute',
        left: 110,
        right: 110,
        bottom: 24,
        display: 'flex',
        justifyContent: 'center',
        pointerEvents: 'none',
        zIndex: 80,
      }}
    >
      <div
        style={{
          maxWidth: 1620,
          padding: '8px 20px',
          borderRadius: 10,
          background: 'rgba(6, 8, 12, 0.84)',
          border: '1px solid rgba(255,255,255,0.08)',
          textAlign: 'center',
          textWrap: 'balance',
          fontFamily: FONT,
          fontSize: 30,
          fontWeight: 550,
          letterSpacing: -0.4,
          lineHeight: 1.24,
          color: C.text,
          textShadow: '0 2px 8px rgba(0,0,0,0.85)',
        }}
      >
        {caption.text}
      </div>
    </div>
  )
}
