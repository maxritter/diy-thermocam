import type { CSSProperties } from 'react'

/**
 * Exact Version 1 Ironbow Thermal Lens mark from website/public/images/brand/logo-mark.svg.
 * Concentric scientific Ironbow thermal rings inside a minimalist rounded camera lens body (zero crosshairs).
 */
export function IronbowMark({
  size = 96,
  ringReveal = 1,
  style,
}: {
  size?: number
  ringReveal?: number
  style?: CSSProperties
}) {
  const s = (offset: number) => Math.max(0, Math.min(1, (ringReveal - offset) / (1 - offset * 0.6)))
  return (
    <svg
      xmlns="http://www.w3.org/2000/svg"
      viewBox="0 0 32 32"
      width={size}
      height={size}
      fill="none"
      role="img"
      aria-label="DIY-Thermocam mark"
      style={style}
    >
      <rect x="2" y="2" width="28" height="28" rx="8.5" fill="#121933" stroke="#2D3A6B" strokeWidth="1" />
      <circle cx="16" cy="16" r={10.8 * s(0.0)} fill="#23377D" />
      <circle cx="16" cy="16" r={8.0 * s(0.15)} fill="#9C2D6E" />
      <circle cx="16" cy="16" r={5.3 * s(0.3)} fill="#EA6D20" />
      <circle cx="16" cy="16" r={2.65 * s(0.45)} fill="#FFE6A3" />
    </svg>
  )
}
