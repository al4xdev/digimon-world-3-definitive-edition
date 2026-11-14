const TE = new TextEncoder();

export function toHex(bytes: Uint8Array): string {
  let out = "";
  for (let i = 0; i < bytes.length; i++) {
    out += bytes[i]!.toString(16).padStart(2, "0");
  }
  return out;
}

export function hexToBytes(hex: string): Uint8Array | null {
  if (hex.length % 2 !== 0 || !/^[0-9a-fA-F]+$/.test(hex)) return null;
  const out = new Uint8Array(hex.length / 2);
  for (let i = 0; i < out.length; i++) {
    out[i] = parseInt(hex.slice(i * 2, i * 2 + 2), 16);
  }
  return out;
}

export function randomTokenHex(byteLen = 32): string {
  const buf = new Uint8Array(byteLen);
  crypto.getRandomValues(buf);
  return toHex(buf);
}

export function randomId(prefix: string): string {
  return `${prefix}_${randomTokenHex(12)}`;
}

/** Rejection-sampled 6-digit code in [000000, 999999] with zero modulo bias. */
export function randomSixDigitCode(): string {
  const max = 1_000_000;
  const limit = Math.floor(0x1_0000_0000 / max) * max;
  const arr = new Uint32Array(1);
  while (true) {
    crypto.getRandomValues(arr);
    const v = arr[0]!;
    if (v < limit) {
      return (v % max).toString().padStart(6, "0");
    }
  }
}

export async function sha256Hex(input: string | Uint8Array | ArrayBuffer): Promise<string> {
  const bytes =
    typeof input === "string"
      ? TE.encode(input)
      : input instanceof ArrayBuffer
        ? new Uint8Array(input)
        : input;
  const digest = await crypto.subtle.digest("SHA-256", bytes);
  return toHex(new Uint8Array(digest));
}

export async function hmacSha256Hex(secret: string, message: string): Promise<string> {
  const key = await crypto.subtle.importKey(
    "raw",
    TE.encode(secret),
    { name: "HMAC", hash: "SHA-256" },
    false,
    ["sign"],
  );
  const sig = await crypto.subtle.sign("HMAC", key, TE.encode(message));
  return toHex(new Uint8Array(sig));
}

export function constantTimeEqualHex(a: string, b: string): boolean {
  const ba = hexToBytes(a);
  const bb = hexToBytes(b);
  if (!ba || !bb || ba.length !== bb.length) return false;
  let diff = 0;
  for (let i = 0; i < ba.length; i++) {
    diff |= ba[i]! ^ bb[i]!;
  }
  return diff === 0;
}

export async function constantTimeEqualStr(a: string, b: string): Promise<boolean> {
  const [ha, hb] = await Promise.all([sha256Hex(a), sha256Hex(b)]);
  return constantTimeEqualHex(ha, hb);
}
