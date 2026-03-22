import { create } from 'zustand';

const initialWallet = null;
const SESSION_KEY = 'walletSession';

function persistWallet(walletData) {
  if (!walletData) {
    localStorage.removeItem(SESSION_KEY);
    return;
  }
  localStorage.setItem(SESSION_KEY, JSON.stringify(walletData));
}

function readPersistedWallet() {
  try {
    const raw = localStorage.getItem(SESSION_KEY);
    if (!raw) return null;
    const parsed = JSON.parse(raw);
    if (!parsed?.privateKey || !parsed?.publicKey || !parsed?.address) {
      return null;
    }
    return parsed;
  } catch {
    return null;
  }
}

export const useWalletStore = create((set) => ({
  wallet: initialWallet,
  isUnlocked: !!initialWallet,

  unlockWallet: (walletData) => {
    persistWallet(walletData);
    set({
      wallet: walletData,
      isUnlocked: true,
    });
  },

  lockWallet: () => {
    persistWallet(null);
    set({
      wallet: null,
      isUnlocked: false,
    });
  },

  hydrate: () => {
    const wallet = readPersistedWallet();
    if (!wallet) {
      set({ wallet: null, isUnlocked: false });
      return;
    }

    set({ wallet, isUnlocked: true });
  }
}));
