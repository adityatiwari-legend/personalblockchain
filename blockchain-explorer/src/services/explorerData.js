export function toEpochMs(timestamp) {
  if (!timestamp && timestamp !== 0) return 0;
  if (typeof timestamp === 'string') {
    const parsed = Date.parse(timestamp);
    if (!Number.isNaN(parsed)) return parsed;
    return 0;
  }
  if (typeof timestamp === 'number') {
    if (timestamp > 1e12) return timestamp;
    if (timestamp > 1e9) return timestamp * 1000;
    return timestamp * 1000;
  }
  return 0;
}

export function formatTimestamp(timestamp) {
  const epoch = toEpochMs(timestamp);
  if (!epoch) return 'N/A';
  return new Date(epoch).toLocaleString();
}

export function normalizeBlock(block, fallbackHeight = 0) {
  const index = block?.index ?? block?.height ?? fallbackHeight;
  const txs = Array.isArray(block?.transactions) ? block.transactions : [];

  return {
    ...block,
    index,
    height: index,
    hash: block?.hash || block?.blockHash || '',
    previousHash: block?.previous_hash || block?.previousHash || '',
    timestamp: block?.timestamp || null,
    difficulty: block?.difficulty ?? '-',
    nonce: block?.nonce ?? '-',
    transactions: txs,
    transactionCount: txs.length,
  };
}

export function normalizeChain(chain = []) {
  return chain.map((block, idx) => normalizeBlock(block, idx));
}

export function txType(tx, walletAddress = '') {
  const sender = tx.sender || tx.fromAddress || tx.from || '';
  const receiver = tx.receiver || tx.toAddress || tx.to || '';
  const type = tx.type || tx.txType;

  if (type === 'reward' || !sender) return 'reward';
  if (walletAddress) {
    if (sender === walletAddress) return 'sent';
    if (receiver === walletAddress) return 'received';
  }
  return 'transfer';
}

export function chainToTransactions(chain = []) {
  const normalizedChain = normalizeChain(chain);
  return normalizedChain.flatMap((block) => {
    const blockHeight = block.index;
    const blockHash = block.hash;

    return block.transactions.map((tx, idx) => ({
      id: tx.txID || tx.id || tx.hash || `${blockHash}-${idx}`,
      txID: tx.txID || tx.id || tx.hash || `${blockHash}-${idx}`,
      sender: tx.sender || tx.fromAddress || tx.from || tx.senderAddress || '',
      receiver: tx.receiver || tx.toAddress || tx.to || tx.receiverAddress || '',
      amount: Number(tx.amount || 0),
      timestamp: tx.timestamp || block.timestamp || null,
      signature: tx.signature || tx.digitalSignature || '',
      payload: tx.payload || '',
      status: tx.status || 'confirmed',
      blockHeight,
      blockHash,
      confirmations: tx.confirmations,
      raw: tx,
    }));
  });
}

export function normalizeTransactions(transactions = [], chain = []) {
  if (!Array.isArray(transactions) || transactions.length === 0) {
    return chainToTransactions(chain);
  }

  return transactions.map((tx, idx) => ({
    id: tx.txID || tx.id || tx.hash || `tx-${idx}`,
    txID: tx.txID || tx.id || tx.hash || `tx-${idx}`,
    sender: tx.sender || tx.fromAddress || tx.from || tx.senderAddress || '',
    receiver: tx.receiver || tx.toAddress || tx.to || tx.receiverAddress || '',
    amount: Number(tx.amount || 0),
    timestamp: tx.timestamp || null,
    signature: tx.signature || tx.digitalSignature || '',
    payload: tx.payload || '',
    status: tx.status || 'confirmed',
    blockHeight: tx.blockHeight ?? tx.blockIndex ?? tx.height ?? null,
    blockHash: tx.blockHash || '',
    confirmations: tx.confirmations,
    raw: tx,
  }));
}

export function shorten(value, head = 10, tail = 8) {
  if (!value || typeof value !== 'string') return 'N/A';
  if (value.length <= head + tail + 3) return value;
  return `${value.slice(0, head)}...${value.slice(-tail)}`;
}
