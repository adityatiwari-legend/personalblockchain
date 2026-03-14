import { create } from 'zustand';

export const useNetworkStore = create((set) => ({
  nodeEndpoint: 'http://localhost:8080',
  networkType: 'mainnet',

  setNodeEndpoint: (endpoint) => set({ nodeEndpoint: endpoint }),
  setNetworkType: (type) => set({ networkType: type }),
}));
