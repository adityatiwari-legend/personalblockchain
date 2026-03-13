import axios from 'axios';

// node1 HTTP API — set VITE_API_BASE_URL in .env before building
const API_BASE_URL = import.meta.env.VITE_API_BASE_URL || 'http://localhost:8001';

const api = axios.create({
  baseURL: API_BASE_URL,
  timeout: 5000,
});

export const blockchainApi = {
  getChain: async () => {
    const response = await api.get('/chain');
    if (response.data && Array.isArray(response.data.chain)) {
      return response.data.chain;
    }
    if (Array.isArray(response.data)) {
      return response.data;
    }
    return [];
  },

  getPeers: async () => {
    const response = await api.get('/peers');
    if (response.data && Array.isArray(response.data.peers)) return response.data.peers;
    if (Array.isArray(response.data)) return response.data;
    return [];
  },

  addTransaction: async (senderPrivateKey, senderPublicKey, receiverPublicKey, payload) => {
    const response = await api.post('/transaction/send', {
      senderPrivateKey,
      senderPublicKey,
      receiverPublicKey,
      payload: String(payload)
    });
    return response.data;
  },

  mineBlock: async (minerAddress) => {
    const body = minerAddress ? { minerAddress } : {};
    const response = await api.post('/mine', body);
    return response.data;
  },

  createWallet: async () => {
    const response = await api.post('/wallet/create');
    return response.data;
  },

  getHealth: async () => {
    const response = await api.get('/health');
    return response.data;
  }
};
