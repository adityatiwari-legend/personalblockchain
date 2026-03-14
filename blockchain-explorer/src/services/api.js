import axios from 'axios';

// node1 HTTP API — set VITE_API_BASE_URL in .env before building
const API_BASE_URL = import.meta.env.VITE_API_BASE_URL || 'http://localhost:8001';

const api = axios.create({
  baseURL: API_BASE_URL,
  timeout: 5000,
});

export const blockchainApi = {
  loginChallenge: async (address, publicKey) => {
    const response = await api.post('/wallet/loginChallenge', { address, publicKey });
    return response.data;
  },

  verifyLogin: async (address, publicKey, challenge, signature) => {
    const response = await api.post('/wallet/verifyLogin', {
      address,
      publicKey,
      challenge,
      signature,
    });
    return response.data;
  },

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

  sendTransaction: async (tx) => {
    const response = await api.post('/transaction/send', tx);
    return response.data;
  },

  addTransaction: async (senderPrivateKey, senderPublicKey, receiverPublicKey, payload) => {
    const response = await api.post('/transaction/send', {
      fromAddress: '',
      toAddress: '',
      senderPublicKey,
      receiverPublicKey,
      amount: 0,
      nonce: 0,
      payload: String(payload),
      signature: senderPrivateKey,
    });
    return response.data;
  },

  getWalletBalance: async (address) => {
    const response = await api.get(`/wallet/balance/${address}`);
    return response.data;
  },

  getWalletTransactions: async (address) => {
    const response = await api.get(`/wallet/transactions/${address}`);
    return response.data;
  },

  mineBlock: async (minerAddress) => {
    const suffix = minerAddress ? `?minerAddress=${encodeURIComponent(minerAddress)}` : '';
    const response = await api.post(`/mine${suffix}`, minerAddress ? { minerAddress } : {});
    return response.data;
  },

  createWallet: async () => {
    const response = await api.post('/wallet/create');
    return response.data;
  },

  importWallet: async (privateKey) => {
    const response = await api.post('/wallet/import', { privateKey });
    return response.data;
  },

  getHealth: async () => {
    const response = await api.get('/health');
    return response.data;
  }
};
