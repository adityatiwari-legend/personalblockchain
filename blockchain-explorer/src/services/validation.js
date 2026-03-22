export const PRIVATE_KEY_REGEX = /^[0-9a-fA-F]{64}$/;
export const ADDRESS_REGEX = /^PCN_[0-9a-fA-F]{40}$/;

export function normalizePrivateKey(value) {
  return String(value || '').trim().toLowerCase();
}

export function isValidPrivateKey(value) {
  return PRIVATE_KEY_REGEX.test(normalizePrivateKey(value));
}

export function normalizeAddress(value) {
  return String(value || '').trim();
}

export function isValidAddress(value) {
  return ADDRESS_REGEX.test(normalizeAddress(value));
}
