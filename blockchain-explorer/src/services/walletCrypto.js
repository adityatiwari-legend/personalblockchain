import { ec as EC } from 'elliptic';

const secp = new EC('secp256k1');

function bytesToHex(bytes) {
  return Array.from(bytes).map((b) => b.toString(16).padStart(2, '0')).join('');
}

export async function sha256Hex(input) {
  const encoded = new TextEncoder().encode(input);
  const digest = await crypto.subtle.digest('SHA-256', encoded);
  return bytesToHex(new Uint8Array(digest));
}

export function derivePublicKey(privateKeyHex) {
  return secp.keyFromPrivate(privateKeyHex, 'hex').getPublic(true, 'hex').toLowerCase();
}

export async function addressFromPublicKey(publicKeyHex) {
  return sha256Hex(publicKeyHex);
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
