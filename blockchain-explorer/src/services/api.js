import axios from 'axios';

// node1 HTTP API is mapped to host port 8001 by docker-compose (8001:8000)
const API_BASE_URL = 'http://localhost:8001';

const api = axios.create({
  baseURL: API_BASE_URL,
  timeout: 5000,
});

export const blockchainApi = {
  getChain: async () => {
    try {
      const response = await api.get('/chain');
      if (response.data && Array.isArray(response.data.chain)) {
        return response.data.chain;
      }
      if (Array.isArray(response.data)) {
        return response.data;
      }
      return [];
    } catch (error) {
      throw error;
    }
  },
  
  getPeers: async () => {
    try {
      const response = await api.get('/peers');
      if (response.data && Array.isArray(response.data.peers)) return response.data.peers;
      if (Array.isArray(response.data)) return response.data;
      return [];
    } catch (error) {
      throw error;
    }
  },
  
  addTransaction: async (sender, receiver, amount) => {
    try {
      const response = await api.post('/addTransaction', { sender, receiver, amount: Number(amount) });
      return response.data;
    } catch (error) {
      try {
        const fallbackRes = await api.post('/transaction/send', { sender, receiver, amount: Number(amount) });
        return fallbackRes.data;
      } catch (e2) {
        throw e2;
      }
    }
  },

  mineBlock: async () => {
    try {
      const response = await api.post('/mine');
      return response.data;
    } catch (error) {
      throw error;
    }
  },

  getHealth: async () => {
    try {
      const response = await api.get('/health');
      return response.data;
    } catch (error) {
      throw error;
    }
  }
};
