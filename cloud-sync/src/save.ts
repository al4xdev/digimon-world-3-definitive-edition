import { PS1_MC_SIZE } from "./types";

export function toUint8Array(value: unknown): Uint8Array {
  if (value instanceof Uint8Array) return value;
  if (value instanceof ArrayBuffer) return new Uint8Array(value);
  if (Array.isArray(value)) return new Uint8Array(value);
  throw new Error("Unsupported BLOB representation from D1");
}

/** Read a stream up to maxBytes + 1 to reject oversized payloads or gzip bombs early. */
export async function readLimitedStream(
  stream: ReadableStream<Uint8Array>,
  maxBytes: number,
): Promise<Uint8Array> {
  const reader = stream.getReader();
  const chunks: Uint8Array[] = [];
  let total = 0;
  try {
    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      if (!value || value.byteLength === 0) continue;
      total += value.byteLength;
      if (total > maxBytes) {
        await reader.cancel("payload_too_large");
        throw new PayloadError("payload_too_large", 413);
      }
      chunks.push(value);
    }
  } finally {
    reader.releaseLock();
  }
  const out = new Uint8Array(total);
  let offset = 0;
  for (const c of chunks) {
    out.set(c, offset);
    offset += c.byteLength;
  }
  return out;
}

export async function gunzipBounded(compressed: Uint8Array, maxBytes: number): Promise<Uint8Array> {
  const ds = new DecompressionStream("gzip");
  const input = new Blob([compressed]).stream().pipeThrough(ds);
  return readLimitedStream(input, maxBytes);
}

export async function gzipBytes(raw: Uint8Array): Promise<Uint8Array> {
  const cs = new CompressionStream("gzip");
  const compressed = new Blob([raw]).stream().pipeThrough(cs);
  return readLimitedStream(compressed, PS1_MC_SIZE + 4096);
}

export function validatePs1MemoryCard(raw: Uint8Array): void {
  if (raw.byteLength !== PS1_MC_SIZE) {
    throw new PayloadError(`invalid_save_size: expected ${PS1_MC_SIZE} bytes, got ${raw.byteLength}`, 400);
  }
  if (raw[0] !== 0x4d || raw[1] !== 0x43) {
    throw new PayloadError("invalid_save_magic: expected PS1 memory card header 'MC'", 400);
  }
}

export class PayloadError extends Error {
  readonly status: number;
  constructor(message: string, status: number) {
    super(message);
    this.status = status;
  }
}
