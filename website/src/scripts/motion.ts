/* Scroll motion for the marketing pages. The layout's inline script adds .motion to <html> before paint
   (unless the visitor asks for reduced motion); without it every element is simply visible. */

const root = document.documentElement;
const reduced = matchMedia('(prefers-reduced-motion: reduce)').matches;
const motion = root.classList.contains('motion') && !reduced;
(window as unknown as { __motionReady?: boolean }).__motionReady = true;

const clamp = (v: number, a = 0, b = 1) => Math.min(b, Math.max(a, v));
const easeOut = (t: number) => 1 - (1 - t) ** 3;

/* Decode: digits and letters roll through random values and settle left to right. */
function scramble(el: HTMLElement) {
  if (!motion) return;
  const final = el.textContent ?? '';
  el.setAttribute('aria-label', final);
  const digits = '0123456789';
  const letters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ';
  const t0 = performance.now();
  const dur = 700;
  const frame = (now: number) => {
    const t = clamp((now - t0) / dur);
    const settled = Math.floor(t * final.length);
    let out = '';
    for (let i = 0; i < final.length; i++) {
      const ch = final[i];
      if (i < settled || !/[0-9A-Za-z]/.test(ch)) out += ch;
      else if (/[0-9]/.test(ch)) out += digits[(Math.random() * 10) | 0];
      else {
        const r = letters[(Math.random() * 26) | 0];
        out += ch === ch.toLowerCase() ? r.toLowerCase() : r;
      }
    }
    el.textContent = out;
    if (t < 1) requestAnimationFrame(frame);
    else el.textContent = final;
  };
  requestAnimationFrame(frame);
}

function wireReveals() {
  const targets = document.querySelectorAll<HTMLElement>(
    '[data-reveal], [data-wipe], [data-scramble]',
  );
  if (!motion) {
    for (const el of targets) el.classList.add('in');
    return;
  }
  const io = new IntersectionObserver(
    (entries) => {
      for (const e of entries) {
        if (!e.isIntersecting) continue;
        const seen = e.target as HTMLElement;
        io.unobserve(seen);
        /* A wipe starts fully clipped, which IntersectionObserver counts as invisible, so its parent is watched. */
        const els = [seen];
        if (seen.hasAttribute('data-wipe-host'))
          els.push(...seen.querySelectorAll<HTMLElement>(':scope > [data-wipe]'));
        for (const el of els) {
          el.classList.add('in');
          if (el.hasAttribute('data-scramble')) scramble(el);
        }
      }
    },
    { rootMargin: '0px 0px -6% 0px', threshold: 0.1 },
  );
  for (const el of targets) {
    const host = el.hasAttribute('data-wipe') ? el.parentElement : null;
    if (host) {
      host.setAttribute('data-wipe-host', '');
      io.observe(host);
    } else io.observe(el);
  }
}

/* Split a heading into lit characters, keeping words together so lines only break between them. */
function splitChars(el: HTMLElement): HTMLElement[] {
  const chars: HTMLElement[] = [];
  el.setAttribute('aria-label', (el.textContent ?? '').replace(/\s+/g, ' ').trim());
  const walk = (node: Node) => {
    for (const child of [...node.childNodes]) {
      if (child.nodeType === Node.TEXT_NODE) {
        const frag = document.createDocumentFragment();
        for (const part of (child.textContent ?? '').split(/(\s+)/)) {
          if (!part) continue;
          if (/^\s+$/.test(part)) {
            frag.append(' ');
            continue;
          }
          const word = document.createElement('span');
          word.className = 'w';
          for (const ch of part) {
            const s = document.createElement('span');
            s.className = 'ch';
            s.textContent = ch;
            word.append(s);
            chars.push(s);
          }
          frag.append(word);
        }
        child.replaceWith(frag);
      } else if (child.nodeType === Node.ELEMENT_NODE) walk(child);
    }
  };
  walk(el);
  for (const c of el.children) c.setAttribute('aria-hidden', 'true');
  return chars;
}

/* The scroll stage: the sentence lights up letter by letter beside a pinned thermal picture,
   which settles into place as it does. */
function wireStage() {
  const stage = document.querySelector<HTMLElement>('[data-stage]');
  if (!stage) return null;
  const text = stage.querySelector<HTMLElement>('[data-chars]');
  if (!motion || !text) {
    stage.classList.add('static');
    return null;
  }
  stage.classList.add('live');
  const chars = splitChars(text);
  const hot = [...text.querySelectorAll<HTMLElement>('.hl')];
  let lastLit = -1;
  const update = () => {
    const r = stage.getBoundingClientRect();
    const span = r.height - innerHeight;
    const p = clamp(-r.top / Math.max(1, span));
    stage.style.setProperty('--p', easeOut(clamp(p / 0.7)).toFixed(3));
    stage.classList.toggle('done', p > 0.72);
    const lit = Math.round(easeOut(clamp((p - 0.02) / 0.62)) * chars.length);
    if (lit !== lastLit) {
      for (const [i, c] of chars.entries()) c.classList.toggle('on', i < lit);
      for (const h of hot) h.classList.toggle('on', !!h.querySelector('.ch.on'));
      lastLit = lit;
    }
  };
  return update;
}

/* [data-scrub] elements get --p from 0 (entering at the bottom) to 1 (reaching the middle). */
function wireScrub() {
  const els = [...document.querySelectorAll<HTMLElement>('[data-scrub]')];
  return () => {
    for (const el of els) {
      const r = el.getBoundingClientRect();
      const p = motion ? clamp((innerHeight - r.top) / (innerHeight * 0.6)) : 1;
      el.style.setProperty('--p', p.toFixed(3));
    }
  };
}

const hdr = document.querySelector<HTMLElement>('.hdr');
const stageUpdate = wireStage();
const scrubUpdate = wireScrub();
wireReveals();

let ticking = false;
const onScroll = () => {
  if (ticking) return;
  ticking = true;
  requestAnimationFrame(() => {
    ticking = false;
    const max = root.scrollHeight - innerHeight;
    if (hdr) hdr.style.setProperty('--read', (max > 0 ? scrollY / max : 0).toFixed(4));
    stageUpdate?.();
    scrubUpdate();
  });
};
addEventListener('scroll', onScroll, { passive: true });
onScroll();
