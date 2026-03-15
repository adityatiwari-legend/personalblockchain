import { useEffect, useMemo, useState } from 'react';
import toast from 'react-hot-toast';
import { copyTextToClipboard } from '../services/clipboard';

function formatSeconds(seconds) {
  const mins = Math.floor(seconds / 60);
  const secs = seconds % 60;
  return `${String(mins).padStart(2, '0')}:${String(secs).padStart(2, '0')}`;
}

export default function WalletCreatedModal({ wallet, onClose }) {
  const [secondsLeft, setSecondsLeft] = useState(120);
  const [confirmedSaved, setConfirmedSaved] = useState(false);

  useEffect(() => {
    const timer = window.setInterval(() => {
      setSecondsLeft((current) => {
        if (current <= 1) {
          window.clearInterval(timer);
          onClose('timeout');
          return 0;
        }
        return current - 1;
      });
    }, 1000);

    return () => {
      window.clearInterval(timer);
    };
  }, [onClose]);

  const countdownLabel = useMemo(() => formatSeconds(secondsLeft), [secondsLeft]);

  const copyToClipboard = async (value) => {
    try {
      await copyTextToClipboard(value);
      toast.success('Copied to clipboard');
    } catch {
      toast.error('Copy failed. Please copy manually.');
    }
  };

  const handleManualClose = () => {
    if (!confirmedSaved) {
      return;
    }
    onClose('confirm');
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/70 p-4">
      <div className="w-full max-w-2xl rounded-3xl border border-rose-500/40 bg-[#0f1729] p-6 shadow-[0_24px_64px_rgba(0,0,0,0.45)]">
        <div className="flex items-start justify-between gap-4">
          <div>
            <p className="text-xs uppercase tracking-[0.2em] text-cyan-300/80">Wallet Created</p>
            <h2 className="mt-2 text-3xl font-black tracking-tight text-white">Save Your Private Key Now</h2>
          </div>
          <div className="rounded-full border border-rose-400/40 bg-rose-500/15 px-4 py-2 text-sm font-semibold text-rose-200">
            {countdownLabel}
          </div>
        </div>

        <div className="mt-5 rounded-2xl border border-rose-500/40 bg-rose-500/10 p-4 text-rose-200">
          <div className="flex items-start gap-3">
            <div className="text-2xl" aria-hidden="true">!</div>
            <div>
              <p className="font-semibold text-rose-100">Critical Security Warning</p>
              <p className="mt-1 text-sm leading-relaxed">
                Save your private key. It cannot be recovered if lost. Do NOT share your private key.
                Store it offline. You need it to login again.
              </p>
            </div>
          </div>
        </div>

        <div className="mt-6 space-y-5">
          <div className="rounded-2xl border border-zinc-700 bg-[#121a2a] p-4">
            <p className="text-xs uppercase tracking-[0.18em] text-zinc-400">Private Key</p>
            <p className="mt-2 break-all font-mono text-sm text-amber-200">{wallet.privateKey}</p>
            <button className="btn-secondary mt-3" onClick={() => copyToClipboard(wallet.privateKey)}>Copy Private Key</button>
          </div>

          <div className="rounded-2xl border border-zinc-700 bg-[#121a2a] p-4">
            <p className="text-xs uppercase tracking-[0.18em] text-zinc-400">Address</p>
            <p className="mt-2 break-all font-mono text-sm text-cyan-200">{wallet.address}</p>
            <button className="btn-secondary mt-3" onClick={() => copyToClipboard(wallet.address)}>Copy Address</button>
          </div>

          <label className="flex items-start gap-3 rounded-xl border border-zinc-700 bg-[#101827] p-3 text-sm text-zinc-200">
            <input
              type="checkbox"
              className="mt-1 h-4 w-4 rounded border-zinc-500"
              checked={confirmedSaved}
              onChange={(e) => setConfirmedSaved(e.target.checked)}
            />
            <span>I saved my private key securely.</span>
          </label>
        </div>

        <div className="mt-6 flex flex-wrap justify-end gap-3">
          <button className="btn-secondary" onClick={handleManualClose} disabled={!confirmedSaved}>
            Continue To Wallet
          </button>
        </div>
      </div>
    </div>
  );
}
