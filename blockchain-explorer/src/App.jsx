import { useEffect } from 'react';
import { Toaster } from 'react-hot-toast';
import { useWalletStore } from './store/useWalletStore';
import AppRoutes from './router/AppRoutes';

export default function App() {
  const hydrate = useWalletStore((state) => state.hydrate);

  useEffect(() => {
    hydrate();
  }, [hydrate]);

  return (
    <>
      <Toaster position="top-right" toastOptions={{ className: 'dark:bg-zinc-800 dark:text-white' }} />
      <AppRoutes />
    </>
  );
}
