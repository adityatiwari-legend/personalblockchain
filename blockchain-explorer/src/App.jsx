import { Navigate, Route, Routes } from 'react-router-dom';
import Login from './pages/Login';
import WalletDashboard from './pages/WalletDashboard';
import Send from './pages/Send';

function requireWallet() {
  const raw = localStorage.getItem('walletSession');
  if (!raw) return null;
  try {
    return JSON.parse(raw);
  } catch {
    return null;
  }
}

function GuardedRoute({ children }) {
  const wallet = requireWallet();
  return wallet ? children : <Navigate to="/login" replace />;
}

export default function App() {
  return (
    <Routes>
      <Route path="/login" element={<Login />} />
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
  );
}
