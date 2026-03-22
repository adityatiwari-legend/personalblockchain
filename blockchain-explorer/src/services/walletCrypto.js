import { ec as EC } from 'elliptic';
import { sha256 as nobleSha256 } from '@noble/hashes/sha2.js';
import { bytesToHex as nobleBytesToHex, utf8ToBytes } from '@noble/hashes/utils.js';

const secp = new EC('secp256k1');

export async function sha256Hex(input) {
  const encoded = new TextEncoder().encode(input);
  const webCrypto = globalThis.crypto;

  // Some server-hosted contexts do not expose crypto.subtle; use a pure JS fallback.
  if (webCrypto?.subtle?.digest) {
    try {
      const digest = await webCrypto.subtle.digest('SHA-256', encoded);
      return nobleBytesToHex(new Uint8Array(digest));
    } catch {
      // Fallback below handles any runtime subtle API failures.
    }
  }

  return nobleBytesToHex(nobleSha256(utf8ToBytes(input)));
}

export function derivePublicKey(privateKeyHex) {
  return secp.keyFromPrivate(privateKeyHex, 'hex').getPublic(true, 'hex').toLowerCase();
}

export async function addressFromPublicKey(publicKeyHex) {
  const digest = await sha256Hex(publicKeyHex);
  return `PCN_${digest.slice(0, 40)}`;
}

export async function signMessage(privateKeyHex, message) {
  const hashHex = await sha256Hex(message);
  const key = secp.keyFromPrivate(privateKeyHex, 'hex');
  const signature = key.sign(hashHex, { canonical: true });
  return signature.toDER('hex');
}

export function currentUtcNoZ() {
  const now = new Date();
  const pad = (n) => String(n).padStart(2, '0');
  return `${now.getUTCFullYear()}-${pad(now.getUTCMonth() + 1)}-${pad(now.getUTCDate())}T${pad(now.getUTCHours())}:${pad(now.getUTCMinutes())}:${pad(now.getUTCSeconds())}`;
}

export async function computeTransactionId(tx) {
  const core = [
    tx.senderPublicKey || '',
    tx.receiverPublicKey || '',
    tx.fromAddress || '',
    tx.toAddress || '',
    String(tx.amount || 0),
    String(tx.nonce || 0),
    tx.payload || '',
    tx.timestamp || '',
  ].join('|');

  return sha256Hex(core);
}
