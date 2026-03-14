import { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { blockchainApi } from '../services/api';
import { addressFromPublicKey, derivePublicKey, signMessage } from '../services/walletCrypto';

export default function Login() {
  const navigate = useNavigate();
  const [privateKey, setPrivateKey] = useState('');
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(false);

  const saveAndLogin = async ({ privateKey, publicKey, address }) => {
    const challengeResp = await blockchainApi.loginChallenge(address, publicKey);
    const signature = await signMessage(privateKey, challengeResp.challenge);
    const loginResp = await blockchainApi.verifyLogin(address, publicKey, challengeResp.challenge, signature);

    localStorage.setItem('walletSession', JSON.stringify({
      privateKey,
      publicKey,
      address,
      token: loginResp.token,
    }));

    navigate('/');
  };

  const handleCreate = async () => {
    try {
      setLoading(true);
      setError('');
      const wallet = await blockchainApi.createWallet();
      await saveAndLogin(wallet);
    } catch (e) {
      setError(e?.response?.data?.error || e.message || 'Create wallet failed');
    } finally {
      setLoading(false);
    }
  };

  const handleImport = async (e) => {
    e.preventDefault();
    try {
      setLoading(true);
      setError('');

      const normalizedPrivate = privateKey.trim().toLowerCase();
      const imported = await blockchainApi.importWallet(normalizedPrivate);
      const derivedPub = derivePublicKey(normalizedPrivate);
      const derivedAddress = await addressFromPublicKey(derivedPub);

      await saveAndLogin({
        privateKey: normalizedPrivate,
        publicKey: imported.publicKey || derivedPub,
        address: imported.address || derivedAddress,
      });
    } catch (e2) {
      setError(e2?.response?.data?.error || e2.message || 'Import wallet failed');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="min-h-screen bg-[radial-gradient(ellipse_at_top,#15314f_0%,#090e18_50%,#060910_100%)] text-white flex items-center justify-center px-6">
      <div className="w-full max-w-xl dashboard-card">
        <h1 className="text-3xl font-black tracking-tight">Wallet Login</h1>
        <p className="text-zinc-400 mt-2">Authenticate with your wallet signature (no password).</p>

        {error && <div className="mt-4 p-3 rounded-xl bg-rose-500/10 border border-rose-500/30 text-rose-300 text-sm">{error}</div>}

        <button className="btn-primary w-full mt-6" onClick={handleCreate} disabled={loading}>
          {loading ? 'Processing...' : 'Create New Wallet'}
        </button>

        <div className="my-6 border-t border-zinc-800" />

        <form onSubmit={handleImport} className="space-y-4">
          <p className="text-sm text-zinc-300">Import Existing Wallet</p>
          <textarea
            className="input-field min-h-[90px]"
            value={privateKey}
            onChange={(e) => setPrivateKey(e.target.value)}
            placeholder="Paste your private key hex"
            required
          />
          <button className="btn-secondary w-full" type="submit" disabled={loading}>
            {loading ? 'Verifying...' : 'Import & Login'}
          </button>
        </form>
      </div>
    </div>
  );
}
