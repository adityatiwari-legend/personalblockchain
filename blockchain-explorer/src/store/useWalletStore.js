import { create } from 'zustand';

const initialWallet = null;

export const useWalletStore = create((set) => ({
  wallet: initialWallet,
  isUnlocked: !!initialWallet,

  unlockWallet: (walletData) => set({
    wallet: walletData,
    isUnlocked: true,
  }),

  lockWallet: () => {
    set({
      wallet: null,
      isUnlocked: false,
    });
  },

  hydrate: () => {
    set({ wallet: null, isUnlocked: false });
  }
}));
