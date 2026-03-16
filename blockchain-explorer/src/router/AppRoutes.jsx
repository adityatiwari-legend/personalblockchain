import { lazy, Suspense } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';
import { useWalletStore } from '../store/useWalletStore';
import Login from '../pages/Login';
import Send from '../pages/Send';
import WalletDashboard from '../pages/WalletDashboard';

const Dashboard = lazy(() => import('../pages/Dashboard'));
const Blocks = lazy(() => import('../pages/Blocks'));
const Transactions = lazy(() => import('../pages/Transactions'));
const Assets = lazy(() => import('../pages/Assets'));
const Profile = lazy(() => import('../pages/Profile'));
const Settings = lazy(() => import('../pages/Settings'));

function GuardedRoute({ children }) {
  const isUnlocked = useWalletStore((state) => state.isUnlocked);
  return isUnlocked ? children : <Navigate to="/login" replace />;
}

function PageFallback() {
  return (
    <div className="flex min-h-screen bg-[#0b0f19] text-gray-200 items-center justify-center">
      <div className="text-center">
        <div className="w-8 h-8 border-2 border-[#00ff9d] border-t-transparent rounded-full animate-spin mx-auto mb-4" />
        <p className="text-gray-400">Loading page...</p>
      </div>
    </div>
  );
}

export default function AppRoutes() {
  const isUnlocked = useWalletStore((state) => state.isUnlocked);

  return (
    <Suspense fallback={<PageFallback />}>
      <Routes>
        <Route path="/login" element={<Login />} />

        <Route
          path="/wallet"
          element={
            <GuardedRoute>
              <WalletDashboard />
            </GuardedRoute>
          }
        />

        <Route
          path="/dashboard"
          element={
            <GuardedRoute>
              <Dashboard />
            </GuardedRoute>
          }
        />

        <Route path="/explorer" element={<Navigate to="/dashboard" replace />} />

        <Route
          path="/blocks"
          element={
            <GuardedRoute>
              <Blocks />
            </GuardedRoute>
          }
        />

        <Route
          path="/transactions"
          element={
            <GuardedRoute>
              <Transactions />
            </GuardedRoute>
          }
        />

        <Route
          path="/assets"
          element={
            <GuardedRoute>
              <Assets />
            </GuardedRoute>
          }
        />

        <Route
          path="/profile"
          element={
            <GuardedRoute>
              <Profile />
            </GuardedRoute>
          }
        />

        <Route
          path="/settings"
          element={
            <GuardedRoute>
              <Settings />
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

        <Route path="/" element={<Navigate to={isUnlocked ? '/wallet' : '/login'} replace />} />
        <Route path="*" element={<Navigate to={isUnlocked ? '/wallet' : '/login'} replace />} />
      </Routes>
    </Suspense>
  );
}
