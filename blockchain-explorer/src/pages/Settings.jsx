import { useEffect, useState } from 'react';
import toast from 'react-hot-toast';
import { useMutation, useQuery } from '@tanstack/react-query';
import ExplorerLayout from '../components/ExplorerLayout';
import { blockchainApi } from '../services/api';
import { useWalletStore } from '../store/useWalletStore';

export default function Settings() {
  const wallet = useWalletStore((state) => state.wallet);
  const lockWallet = useWalletStore((state) => state.lockWallet);
  const unlockWallet = useWalletStore((state) => state.unlockWallet);

  const [nodeName, setNodeName] = useState('');
  const [port, setPort] = useState('');
  const [maxPeers, setMaxPeers] = useState('');
  const [importKey, setImportKey] = useState('');
  const [exportedKey, setExportedKey] = useState('');

  const configQuery = useQuery({
    queryKey: ['node-config'],
    queryFn: blockchainApi.getNodeConfig,
    retry: 2,
  });

  useEffect(() => {
    const cfg = configQuery.data || {};
    setNodeName(String(cfg.nodeName || cfg.name || ''));
    setPort(String(cfg.port || cfg.httpPort || ''));
    setMaxPeers(String(cfg.maxPeers || cfg.peerLimit || ''));
  }, [configQuery.data]);

  const saveConfigMutation = useMutation({
    mutationFn: () =>
      blockchainApi.updateNodeConfig({
        nodeName,
        port: Number(port),
        maxPeers: Number(maxPeers),
      }),
    onSuccess: () => {
      toast.success('Node configuration updated');
      configQuery.refetch();
    },
    onError: () => toast.error('Failed to save node configuration'),
  });

  const exportKeyMutation = useMutation({
    mutationFn: () => blockchainApi.exportPrivateKey(wallet?.address || ''),
    onSuccess: (data) => {
      const key = data?.privateKey || data?.key || '';
      setExportedKey(key);
      toast.success('Private key exported');
    },
    onError: () => toast.error('Private key export failed'),
  });

  const importWalletMutation = useMutation({
    mutationFn: () => blockchainApi.importWallet(importKey.trim()),
    onSuccess: (data) => {
      unlockWallet({
        privateKey: importKey.trim(),
        publicKey: data.publicKey || wallet?.publicKey || '',
        address: data.address || wallet?.address || '',
        token: wallet?.token || '',
      });
      toast.success('Wallet imported');
      setImportKey('');
    },
    onError: () => toast.error('Wallet import failed'),
  });

  const resetWalletMutation = useMutation({
    mutationFn: blockchainApi.resetWallet,
    onSuccess: () => {
      lockWallet();
      toast.success('Wallet reset');
    },
    onError: () => toast.error('Wallet reset failed'),
  });

  const disconnectMutation = useMutation({
    mutationFn: blockchainApi.disconnectNode,
    onSuccess: () => toast.success('Node disconnected'),
    onError: () => toast.error('Node disconnect failed'),
  });

  return (
    <ExplorerLayout section="Settings">
      <div className="space-y-6">
        <div>
          <h2 className="text-2xl font-bold text-white">Settings</h2>
          <p className="text-sm text-gray-400">Node and wallet configuration controls.</p>
        </div>

        <div className="dashboard-card space-y-4">
          <p className="text-xs uppercase tracking-[0.2em] text-cyan-300/80">Node Configuration</p>
          <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
            <input className="input-field" placeholder="Node name" value={nodeName} onChange={(e) => setNodeName(e.target.value)} />
            <input className="input-field" placeholder="Port" value={port} onChange={(e) => setPort(e.target.value)} />
            <input className="input-field" placeholder="Max peers" value={maxPeers} onChange={(e) => setMaxPeers(e.target.value)} />
          </div>
          <div className="flex gap-2">
            <button type="button" className="btn-primary" onClick={() => saveConfigMutation.mutate()} disabled={saveConfigMutation.isPending}>
              {saveConfigMutation.isPending ? 'Saving...' : 'Save Config'}
            </button>
            <button type="button" className="btn-secondary" onClick={() => configQuery.refetch()}>
              Reload
            </button>
          </div>
          {configQuery.isError && <p className="text-sm text-red-300">Failed to load node config.</p>}
        </div>

        <div className="dashboard-card space-y-4">
          <p className="text-xs uppercase tracking-[0.2em] text-cyan-300/80">Wallet Options</p>
          <div className="flex flex-wrap gap-2">
            <button
              type="button"
              className="btn-secondary"
              onClick={() => exportKeyMutation.mutate()}
              disabled={!wallet?.address || exportKeyMutation.isPending}
            >
              {exportKeyMutation.isPending ? 'Exporting...' : 'Export Private Key'}
            </button>
          </div>

          {exportedKey && (
            <div className="p-3 rounded-lg bg-[#0b0f19] border border-gray-800">
              <p className="text-xs text-gray-400 mb-1">Exported key</p>
              <p className="font-mono text-xs break-all text-gray-300">{exportedKey}</p>
            </div>
          )}

          <div className="space-y-2">
            <textarea
              className="input-field min-h-[90px]"
              placeholder="Import wallet: paste private key"
              value={importKey}
              onChange={(e) => setImportKey(e.target.value)}
            />
            <button
              type="button"
              className="btn-secondary"
              onClick={() => importWalletMutation.mutate()}
              disabled={!importKey.trim() || importWalletMutation.isPending}
            >
              {importWalletMutation.isPending ? 'Importing...' : 'Import Wallet'}
            </button>
          </div>
        </div>

        <div className="dashboard-card space-y-3 border-red-500/30 bg-red-500/5">
          <p className="text-xs uppercase tracking-[0.2em] text-red-300">Danger Zone</p>
          <div className="flex flex-wrap gap-2">
            <button
              type="button"
              className="bg-red-600/80 text-white font-semibold rounded-xl px-4 py-2 hover:bg-red-600 transition-all"
              onClick={() => resetWalletMutation.mutate()}
              disabled={resetWalletMutation.isPending}
            >
              {resetWalletMutation.isPending ? 'Resetting...' : 'Reset Wallet'}
            </button>
            <button
              type="button"
              className="bg-red-600/80 text-white font-semibold rounded-xl px-4 py-2 hover:bg-red-600 transition-all"
              onClick={() => disconnectMutation.mutate()}
              disabled={disconnectMutation.isPending}
            >
              {disconnectMutation.isPending ? 'Disconnecting...' : 'Disconnect Node'}
            </button>
          </div>
        </div>
      </div>
    </ExplorerLayout>
  );
}
