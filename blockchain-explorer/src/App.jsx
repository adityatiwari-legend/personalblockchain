import { useEffect } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';
import { Toaster } from 'react-hot-toast';
import { useWalletStore } from './store/useWalletStore';
import Login from './pages/Login';
import WalletDashboard from './pages/WalletDashboard';
import Send from './pages/Send';
import Dashboard from './pages/Dashboard';

function GuardedRoute({ children }) {
  const isUnlocked = useWalletStore((state) => state.isUnlocked);
  return isUnlocked ? children : <Navigate to="/login" replace />;
}

export default function App() {
  const hydrate = useWalletStore((state) => state.hydrate);

  useEffect(() => {
    hydrate();
  }, [hydrate]);

  return (
    <>
      <Toaster position="top-right" toastOptions={{ className: 'dark:bg-zinc-800 dark:text-white' }} />
      <Routes>
        <Route path="/login" element={<Login />} />
        <Route path="/explorer" element={<Dashboard />} />
        <Route
          path="/"
          element={
            <GuardedRoute>
              <WalletDashboard />
            </GuardedRoute>
          }
        />
        <Route
          path="/send"
          element={
            <GuardedRoute>
              <Send />
            </GuardedRoute>
          }
        />
        <Route path="*" element={<Navigate to="/" replace />} />
      </Routes>
    </>
  );
}
