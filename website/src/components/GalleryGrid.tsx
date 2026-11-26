import { ChevronLeft, ChevronRight, X } from 'lucide-react';
import { useEffect, useMemo, useState } from 'react';

export interface GalleryItem {
  title: string;
  caption?: string;
  image: string;
  category?: string;
  fit?: 'cover' | 'contain';
}

interface Props {
  items: GalleryItem[];
  categories?: string[];
  aspect?: '4/3' | '16/9' | '16/10' | 'auto';
}

export default function GalleryGrid({ items, categories, aspect = '4/3' }: Props) {
  const [selectedCategory, setSelectedCategory] = useState<string>('All');
  const [activeIndex, setActiveIndex] = useState<number | null>(null);

  const visibleItems = useMemo(() => {
    if (!categories || selectedCategory === 'All') {
      return items;
    }
    return items.filter((item) => item.category === selectedCategory);
  }, [items, categories, selectedCategory]);

  useEffect(() => {
    if (activeIndex === null) {
      return;
    }
    const onKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        setActiveIndex(null);
      } else if (e.key === 'ArrowRight') {
        setActiveIndex((idx) => (idx === null ? null : (idx + 1) % visibleItems.length));
      } else if (e.key === 'ArrowLeft') {
        setActiveIndex((idx) =>
          idx === null ? null : (idx - 1 + visibleItems.length) % visibleItems.length,
        );
      }
    };
    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, [activeIndex, visibleItems.length]);

  const activeItem = activeIndex !== null ? visibleItems[activeIndex] : null;

  return (
    <div>
      {categories && categories.length > 0 && (
        <div
          style={{
            display: 'flex',
            flexWrap: 'wrap',
            gap: '8px',
            marginBottom: '24px',
          }}
          role="tablist"
          aria-label="Filter gallery by category"
        >
          {['All', ...categories].map((cat) => {
            const active = selectedCategory === cat;
            return (
              <button
                key={cat}
                type="button"
                role="tab"
                aria-selected={active}
                onClick={() => {
                  setSelectedCategory(cat);
                  setActiveIndex(null);
                }}
                style={{
                  padding: '7px 14px',
                  borderRadius: '8px',
                  fontSize: '13.5px',
                  fontWeight: active ? 600 : 500,
                  cursor: 'pointer',
                  border: active
                    ? '1px solid var(--tc-border-strong)'
                    : '1px solid var(--tc-border)',
                  background: active ? 'var(--tc-surface-2)' : 'var(--tc-surface)',
                  color: active ? 'var(--tc-text)' : 'var(--tc-text-muted)',
                }}
              >
                {cat}
              </button>
            );
          })}
        </div>
      )}

      <div
        style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fill, minmax(290px, 1fr))',
          gap: '24px',
        }}
      >
        {visibleItems.map((item, idx) => (
          <figure
            key={item.image}
            style={{
              margin: 0,
              display: 'flex',
              flexDirection: 'column',
              gap: '8px',
            }}
          >
            <button
              type="button"
              onClick={() => setActiveIndex(idx)}
              aria-label={`Inspect ${item.title} full size`}
              style={{
                border: '1px solid var(--tc-border)',
                borderRadius: '8px',
                padding: 0,
                background: 'var(--tc-bg-sunken)',
                cursor: 'zoom-in',
                overflow: 'hidden',
                aspectRatio: aspect === 'auto' ? undefined : aspect,
                display: 'block',
                width: '100%',
              }}
            >
              <img
                src={item.image}
                alt={item.caption ? `${item.title} — ${item.caption}` : item.title}
                loading="lazy"
                style={{
                  width: '100%',
                  height: '100%',
                  objectFit: item.fit ?? 'cover',
                  display: 'block',
                }}
              />
            </button>
            <figcaption style={{ padding: '2px 0' }}>
              <div
                style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'baseline',
                  gap: '8px',
                }}
              >
                <span style={{ fontSize: '14.5px', fontWeight: 600, color: 'var(--tc-text)' }}>
                  {item.title}
                </span>
                {item.category && (
                  <span style={{ fontSize: '11.5px', color: 'var(--tc-text-dim)' }}>
                    {item.category}
                  </span>
                )}
              </div>
              {item.caption && (
                <p style={{ margin: '4px 0 0', fontSize: '13px', color: 'var(--tc-text-muted)' }}>
                  {item.caption}
                </p>
              )}
            </figcaption>
          </figure>
        ))}
      </div>

      {activeItem && activeIndex !== null && (
        <div
          role="dialog"
          aria-modal="true"
          aria-label={activeItem.title}
          style={{
            position: 'fixed',
            inset: 0,
            zIndex: 100,
            background: 'rgba(10, 12, 16, 0.88)',
            display: 'grid',
            placeItems: 'center',
            padding: '24px',
          }}
        >
          <button
            type="button"
            aria-label="Close image preview backdrop"
            onClick={() => setActiveIndex(null)}
            style={{
              position: 'absolute',
              inset: 0,
              border: 0,
              background: 'transparent',
              cursor: 'default',
            }}
          />
          <div
            style={{
              position: 'relative',
              zIndex: 1,
              maxWidth: '1080px',
              width: '100%',
              background: 'var(--tc-surface)',
              border: '1px solid var(--tc-border)',
              borderRadius: '12px',
              overflow: 'hidden',
              boxShadow: 'var(--tc-shadow-window)',
            }}
          >
            <div
              style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                padding: '12px 16px',
                borderBottom: '1px solid var(--tc-border)',
              }}
            >
              <div>
                <strong style={{ fontSize: '15px', color: 'var(--tc-text)' }}>
                  {activeItem.title}
                </strong>
                {activeItem.caption && (
                  <span
                    style={{ marginLeft: '10px', fontSize: '13px', color: 'var(--tc-text-muted)' }}
                  >
                    {activeItem.caption}
                  </span>
                )}
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                <button
                  type="button"
                  aria-label="Previous image"
                  onClick={() =>
                    setActiveIndex((activeIndex - 1 + visibleItems.length) % visibleItems.length)
                  }
                  style={{
                    width: '32px',
                    height: '32px',
                    display: 'inline-grid',
                    placeItems: 'center',
                    borderRadius: '6px',
                    border: '1px solid var(--tc-border)',
                    background: 'var(--tc-bg-sunken)',
                    color: 'var(--tc-text)',
                    cursor: 'pointer',
                  }}
                >
                  <ChevronLeft size={16} />
                </button>
                <button
                  type="button"
                  aria-label="Next image"
                  onClick={() => setActiveIndex((activeIndex + 1) % visibleItems.length)}
                  style={{
                    width: '32px',
                    height: '32px',
                    display: 'inline-grid',
                    placeItems: 'center',
                    borderRadius: '6px',
                    border: '1px solid var(--tc-border)',
                    background: 'var(--tc-bg-sunken)',
                    color: 'var(--tc-text)',
                    cursor: 'pointer',
                  }}
                >
                  <ChevronRight size={16} />
                </button>
                <button
                  type="button"
                  aria-label="Close preview"
                  onClick={() => setActiveIndex(null)}
                  style={{
                    width: '32px',
                    height: '32px',
                    display: 'inline-grid',
                    placeItems: 'center',
                    borderRadius: '6px',
                    border: '1px solid var(--tc-border)',
                    background: 'var(--tc-bg-sunken)',
                    color: 'var(--tc-text)',
                    cursor: 'pointer',
                  }}
                >
                  <X size={16} />
                </button>
              </div>
            </div>
            <div
              style={{
                background: '#050608',
                display: 'grid',
                placeItems: 'center',
                maxHeight: '78vh',
              }}
            >
              <img
                src={activeItem.image}
                alt={activeItem.title}
                style={{
                  maxWidth: '100%',
                  maxHeight: '78vh',
                  objectFit: 'contain',
                  display: 'block',
                }}
              />
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
