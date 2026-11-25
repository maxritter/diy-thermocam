/**
 * Fresh 2026 DIY-Thermocam brand mark:
 * Minimalist rounded thermal-camera lens aperture with concentric heat-gradient rings
 * (deep indigo -> crimson/magenta -> warm thermal amber -> soft ivory core).
 * Clean scientific open-hardware aesthetic with zero crosshairs or reticle ticks.
 */
export function BrandMark({ size = 24 }: { size?: number }): React.JSX.Element {
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 32 32"
      fill="none"
      xmlns="http://www.w3.org/2000/svg"
      aria-hidden="true"
    >
      <defs>
        <radialGradient id="tc-lens-glow" cx="50%" cy="50%" r="50%">
          <stop offset="0%" stopColor="#fff7e0" />
          <stop offset="28%" stopColor="#f7b23b" />
          <stop offset="56%" stopColor="#d94662" />
          <stop offset="82%" stopColor="#5b2178" />
          <stop offset="100%" stopColor="#1e1b4b" />
        </radialGradient>
        <linearGradient
          id="tc-shell-stroke"
          x1="3"
          y1="3"
          x2="29"
          y2="29"
          gradientUnits="userSpaceOnUse"
        >
          <stop offset="0%" stopColor="#f7b23b" stopOpacity="0.65" />
          <stop offset="50%" stopColor="#d94662" stopOpacity="0.45" />
          <stop offset="100%" stopColor="#5da3e5" stopOpacity="0.5" />
        </linearGradient>
      </defs>
      {/* Rounded camera body / lens housing */}
      <rect
        x="2"
        y="2"
        width="28"
        height="28"
        rx="8.5"
        fill="#131126"
        stroke="url(#tc-shell-stroke)"
        strokeWidth="1.4"
      />
      {/* Concentric thermal aperture rings */}
      <circle cx="16" cy="16" r="10.2" fill="url(#tc-lens-glow)" opacity="0.22" />
      <circle cx="16" cy="16" r="9.8" stroke="#433899" strokeWidth="1.6" />
      <circle cx="16" cy="16" r="7.1" stroke="#b92b6c" strokeWidth="1.75" />
      <circle cx="16" cy="16" r="4.5" stroke="#f5821f" strokeWidth="1.8" />
      {/* Warm ivory thermal core */}
      <circle cx="16" cy="16" r="2.2" fill="#fff7e0" />
    </svg>
  );
}
