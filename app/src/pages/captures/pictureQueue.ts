/**
 * Card pictures load through a queue: at most `limit` at a time (each one is a read over USB
 * for the camera), in the order given (newest first), and dropped when the tab changes.
 */
export interface TaskQueue {
  /** Adds tasks in order; a task already queued or running under the same key is skipped. */
  add(tasks: { key: string; run: () => Promise<void> }[]): void;
  /** Forgets every task that has not started. Running ones finish but their results are ignored. */
  clear(): void;
  /** Tasks started or waiting. */
  pending(): number;
}

export function createTaskQueue(limit: number): TaskQueue {
  let waiting: { key: string; run: () => Promise<void> }[] = [];
  const running = new Set<string>();
  let generation = 0;

  const pump = () => {
    while (running.size < limit && waiting.length > 0) {
      const next = waiting.shift();
      if (!next) break;
      const gen = generation;
      running.add(next.key);
      void next
        .run()
        .catch(() => {})
        .finally(() => {
          running.delete(next.key);
          if (gen === generation || running.size < limit) pump();
        });
    }
  };

  return {
    add(tasks) {
      const known = new Set([...running, ...waiting.map((t) => t.key)]);
      for (const t of tasks) {
        if (known.has(t.key)) continue;
        known.add(t.key);
        waiting.push(t);
      }
      pump();
    },
    clear() {
      generation++;
      waiting = [];
    },
    pending() {
      return running.size + waiting.length;
    },
  };
}
