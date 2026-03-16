import axios from 'axios';
import toast from 'react-hot-toast';

// node1 HTTP API — set VITE_API_BASE_URL in .env before building
const API_BASE_URL = import.meta.env.VITE_API_BASE_URL || 'http://localhost:8001';

const api = axios.create({
  baseURL: API_BASE_URL,
  timeout: 5000,
});

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

async function withRetry(requestFn, retries = 2, delayMs = 350) {
  let lastError;
  for (let attempt = 0; attempt <= retries; attempt += 1) {
    try {
      return await requestFn();
    } catch (error) {
      lastError = error;
      if (attempt >= retries) break;
      await sleep(delayMs * (attempt + 1));
    }
  }
  throw lastError;
}

function normalizeChainResponse(data) {
  if (data && Array.isArray(data.chain)) return data.chain;
  if (Array.isArray(data)) return data;
  return [];
}

function txFromBlock(block) {
  const txs = Array.isArray(block?.transactions) ? block.transactions : [];
  const blockHeight = block?.index ?? block?.height ?? null;
  const blockHash = block?.hash ?? '';
  return txs.map((tx, idx) => ({
    id: tx.txID || tx.id || tx.hash || `${blockHash}-${idx}`,
    txID: tx.txID || tx.id || tx.hash || `${blockHash}-${idx}`,
    sender: tx.sender || tx.fromAddress || tx.from || tx.senderAddress || '',
    receiver: tx.receiver || tx.toAddress || tx.to || tx.receiverAddress || '',
    amount: Number(tx.amount || 0),
    timestamp: tx.timestamp || block?.timestamp || null,
    signature: tx.signature || '',
    payload: tx.payload || '',
    status: tx.status || 'confirmed',
    type: tx.type || tx.txType || (tx.sender || tx.fromAddress ? 'transfer' : 'reward'),
    blockHeight,
    blockHash,
    confirmations: tx.confirmations,
    raw: tx,
  }));
}

api.interceptors.response.use(
  (response) => response,
  (error) => {
    if (error.response?.data?.error) {
      toast.error(error.response.data.error);
    } else if (error.message) {
      toast.error(error.message);
    }
    return Promise.reject(error);
  }
);

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
    const response = await withRetry(() => api.get('/chain'));
    return normalizeChainResponse(response.data);
  },

  getTransactions: async () => {
    try {
      const response = await withRetry(() => api.get('/transactions'));
      if (response.data && Array.isArray(response.data.transactions)) {
        return response.data.transactions;
      }
      if (Array.isArray(response.data)) {
        return response.data;
      }
    } catch {
      // Fallback below derives transaction history from chain.
    }

    const chain = await blockchainApi.getChain();
    return chain.flatMap((block) => txFromBlock(block));
  },

  getPeers: async () => {
    const response = await withRetry(() => api.get('/peers'));
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
    const path = address ? `/wallet/balance/${address}` : '/wallet/balance';
    const response = await withRetry(() => api.get(path));
    return response.data;
  },

  getWalletTransactions: async (address) => {
    const path = address ? `/wallet/transactions/${address}` : '/wallet/transactions';
    const response = await withRetry(() => api.get(path));
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
    const response = await withRetry(() => api.get('/health'));
    return response.data;
  },

  getStatus: async () => {
    const response = await withRetry(() => api.get('/status'));
    return response.data;
  },

  getMempool: async () => {
    const response = await withRetry(() => api.get('/mempool'));
    if (response.data && Array.isArray(response.data.transactions)) {
      return response.data.transactions;
    }
    if (Array.isArray(response.data)) {
      return response.data;
    }
    return [];
  },

  getNodeConfig: async () => {
    const response = await withRetry(() => api.get('/config'));
    return response.data;
  },

  updateNodeConfig: async (config) => {
    const response = await withRetry(() => api.post('/config', config));
    return response.data;
  },

  exportPrivateKey: async (address) => {
    const response = await withRetry(() => api.get(`/wallet/export/${encodeURIComponent(address)}`));
    return response.data;
  },

  resetWallet: async () => {
    const response = await withRetry(() => api.post('/wallet/reset'));
    return response.data;
  },

  disconnectNode: async () => {
    const response = await withRetry(() => api.post('/network/disconnect'));
    return response.data;
  }
};
