import { useState } from 'react';

export default function SendForm({ onSubmit, sending, nextNonce }) {
  const [toAddress, setToAddress] = useState('');
  const [amount, setAmount] = useState('');
  const [payload, setPayload] = useState('');

  const submit = async (e) => {
    e.preventDefault();
    const success = await onSubmit({
      toAddress: toAddress.trim(),
      amount: Number(amount),
      payload: payload.trim(),
      nonce: Number(nextNonce),
    });
    if (success) {
      setAmount('');
      setPayload('');
    }
  };

  return (
    <form onSubmit={submit} className="dashboard-card space-y-4">
      <p className="text-xs uppercase tracking-[0.2em] text-cyan-300/80">Send Coins</p>
      <input className="input-field" placeholder="Receiver Address" value={toAddress} onChange={(e) => setToAddress(e.target.value)} required />
      <input className="input-field" placeholder="Amount" type="number" min="1" value={amount} onChange={(e) => setAmount(e.target.value)} required />
      <input className="input-field" placeholder="Memo / Payload" value={payload} onChange={(e) => setPayload(e.target.value)} maxLength={256} />
      <div className="text-xs text-zinc-400">Nonce: {nextNonce}</div>
      <button className="btn-primary w-full" type="submit" disabled={sending}>{sending ? 'Sending...' : 'Sign & Send'}</button>
    </form>
  );
}
