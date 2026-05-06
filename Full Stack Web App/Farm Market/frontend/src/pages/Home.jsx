import React from 'react';
import { Link } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { AppShell } from '../components';

const BUYER_ICONS = [
  { to: '/marketplace', icon: '🏪', label: 'Marketplace' },
  { to: '/orders',      icon: '🚚', label: 'My Orders' },
  { to: '/reviews',     icon: '⭐', label: 'Reviews' },
  { to: '/membership',  icon: '🎖', label: 'Membership' },
  { to: '/profile',     icon: '⚙️', label: 'Settings' },
];

const SELLER_ICONS = [
  { to: '/listings',  icon: '🏪', label: 'My Listings' },
  { to: '/incoming',  icon: '🚚', label: 'Incoming Orders' },
  { to: '/reviews',   icon: '⭐', label: 'Reviews' },
  { to: '/membership',icon: '🎖', label: 'Membership' },
  { to: '/profile',   icon: '⚙️', label: 'Settings' },
];

export default function Home() {
  const { user } = useAuth();
  const isBuyer = user?.role === 'buyer';
  const icons = isBuyer ? BUYER_ICONS : SELLER_ICONS;

  return (
    <AppShell>
      {/* Farm background */}
      <div style={{
        position: 'fixed', inset: 0, zIndex: -1,
        background: 'linear-gradient(180deg,#f0faf4 0%,#d4edda 100%)',
      }} />

      <div style={{ maxWidth: 700, margin: '0 auto' }}>
        {/* Welcome header */}
        <div style={{
          background: 'rgba(255,255,255,0.85)',
          borderRadius: 'var(--radius-lg)',
          padding: '24px 32px',
          marginBottom: 32,
          boxShadow: 'var(--shadow-sm)',
          border: '1px solid var(--gray-200)',
        }}>
          <h1 style={{ fontFamily: 'var(--font-display)', fontWeight: 800, fontSize: 28, color: 'var(--gray-800)', marginBottom: 4 }}>
            Welcome back 👋
          </h1>
          <p style={{ color: 'var(--gray-600)', fontSize: 15 }}>
            {user?.email} · {isBuyer ? 'Buyer' : 'Seller'} Account
          </p>
        </div>

        {/* Icon grid */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fill, minmax(130px, 1fr))',
          gap: 20,
        }}>
          {icons.map(({ to, icon, label }) => (
            <Link key={to} to={to} className="home-icon-card">
              <span className="home-icon">{icon}</span>
              <span className="home-icon-label">{label}</span>
            </Link>
          ))}
        </div>

        {/* Quick action for seller */}
        {!isBuyer && (
          <div style={{ marginTop: 32 }}>
            <Link to="/listings/new" className="btn btn-green-light" style={{ width: '100%', justifyContent: 'center' }}>
              + Create New Listing
            </Link>
          </div>
        )}

        {/* Quick action for buyer */}
        {isBuyer && (
          <div style={{ marginTop: 32 }}>
            <Link to="/marketplace" className="btn btn-green btn-lg btn-full">
              Browse the Marketplace →
            </Link>
          </div>
        )}
      </div>
    </AppShell>
  );
}
