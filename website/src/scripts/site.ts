import { ICONS } from '../lib/icons';
import { paintDownload } from './download';

const root = document.documentElement;

/* Theme: the same localStorage key Starlight uses, so the site and /docs/ stay in step. */
function currentTheme(): 'light' | 'dark' {
  const t = root.dataset.theme;
  if (t === 'dark' || t === 'light') return t;
  return matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
}
function paintToggles() {
  const dark = currentTheme() === 'dark';
  for (const b of document.querySelectorAll<HTMLElement>('[data-theme-toggle]')) {
    b.innerHTML = dark ? ICONS.sun : ICONS.moon;
    b.setAttribute('aria-label', dark ? 'Switch to light theme' : 'Switch to dark theme');
  }
}
function setTheme(t: 'light' | 'dark') {
  root.dataset.theme = t;
  try {
    localStorage.setItem('starlight-theme', t);
  } catch {
    /* Private windows can refuse storage; the theme still applies to this page. */
  }
  paintToggles();
}

/* Tabs: click or arrow keys. */
function wireTabs() {
  for (const list of document.querySelectorAll<HTMLElement>('[role=tablist]')) {
    const tabs = [...list.querySelectorAll<HTMLElement>('[role=tab]')];
    const select = (tab: HTMLElement) => {
      for (const t of tabs) {
        const on = t === tab;
        t.setAttribute('aria-selected', on ? 'true' : 'false');
        t.tabIndex = on ? 0 : -1;
        const panel = document.getElementById(t.getAttribute('aria-controls') || '');
        if (panel) panel.hidden = !on;
      }
    };
    tabs.forEach((t, i) => {
      t.addEventListener('click', () => select(t));
      t.addEventListener('keydown', (e) => {
        const n = e.key === 'ArrowRight' ? 1 : e.key === 'ArrowLeft' ? -1 : 0;
        if (!n) return;
        e.preventDefault();
        const next = tabs[(i + n + tabs.length) % tabs.length];
        select(next);
        next.focus();
      });
    });
  }
}

function wireMenu() {
  const mb = document.querySelector<HTMLButtonElement>('.menu-btn');
  const mn = document.getElementById('mnav');
  if (!mb || !mn) return;
  mb.addEventListener('click', () => {
    const open = mn.hidden;
    mn.hidden = !open;
    mb.setAttribute('aria-expanded', open ? 'true' : 'false');
  });
  mn.addEventListener('click', (e) => {
    if ((e.target as HTMLElement).closest('a')) {
      mn.hidden = true;
      mb.setAttribute('aria-expanded', 'false');
    }
  });
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape' && !mn.hidden) {
      mn.hidden = true;
      mb.setAttribute('aria-expanded', 'false');
      mb.focus();
    }
  });
}

paintToggles();
paintDownload();
wireTabs();
wireMenu();
for (const b of document.querySelectorAll('[data-theme-toggle]')) {
  b.addEventListener('click', () => setTheme(currentTheme() === 'dark' ? 'light' : 'dark'));
}
