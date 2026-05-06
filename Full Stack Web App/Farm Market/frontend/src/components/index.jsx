import React, { useState } from 'react';
import { Link, NavLink, useNavigate, Navigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';

// ── Navbar ────────────────────────────────────
export function Navbar({ onMenuClick }) {
  const { user, logout } = useAuth();
  const navigate = useNavigate();

  const handleLogout = () => {
    logout();
    navigate('/');
  };

  return (
    <nav className="navbar">
      <span className="navbar-brand">
        {user?.role === 'seller' ? "Farmer's Market (Seller)" : "Farmer's Market (Buyer)"}
      </span>
      <Link
        to={user ? '/home' : '/'}
        className="navbar-logo"
        style={{ textDecoration: 'none', color: '#fff' }}
      >
        🌾 FarmMarket
      </Link>
      <div className="navbar-right">
        {user?.role === 'buyer' && (
          <Link to="/cart" className="navbar-cart-btn">🛒 Cart</Link>
        )}
        {user && (
          <button
            onClick={handleLogout}
            style={{
              background: 'rgba(255,255,255,0.1)',
              color: '#fff',
              border: '1px solid rgba(255,255,255,0.2)',
              borderRadius: 6,
              padding: '5px 12px',
              fontSize: 12,
              cursor: 'pointer',
            }}
          >
            Logout
          </button>
        )}
        <button className="navbar-menu-btn" onClick={onMenuClick}>
          <span /><span /><span />
        </button>
      </div>
    </nav>
  );
}

// ── Sidebar ───────────────────────────────────
export function Sidebar({ open, onClose }) {
  const { user } = useAuth();
  const isBuyer  = user?.role === 'buyer';

  const buyerLinks = [
    { to: '/home',        label: 'HOME' },
    { to: '/profile',     label: 'PROFILE' },
    { to: '/marketplace', label: 'MARKETPLACE' },
    { to: '/orders',      label: 'MY ORDERS' },
    { to: '/reviews',     label: 'REVIEWS' },
    { to: '/membership',  label: 'MEMBERSHIP' },
  ];

  const sellerLinks = [
    { to: '/home',      label: 'HOME' },
    { to: '/profile',   label: 'PROFILE' },
    { to: '/listings',  label: 'MY LISTINGS' },
    { to: '/incoming',  label: 'INCOMING ORDERS' },
    { to: '/reviews',   label: 'REVIEWS' },
    { to: '/membership',label: 'MEMBERSHIP' },
  ];

  const links = isBuyer ? buyerLinks : sellerLinks;

  if (!open) return null;
  return (
    <>
      <div
        onClick={onClose}
        style={{
          position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.4)',
          zIndex: 150,
        }}
      />
      <aside style={{
        position: 'fixed', top: 0, left: 0, bottom: 0,
        width: 220, background: 'linear-gradient(180deg,#1e1e20 0%,#2a2a2e 100%)',
        zIndex: 200, padding: '24px 0',
        boxShadow: '4px 0 24px rgba(0,0,0,0.4)',
        transition: 'transform 0.25s ease',
      }}>
        <div style={{ padding: '0 20px 20px', borderBottom: '1px solid rgba(255,255,255,0.08)' }}>
          <div style={{ fontFamily: 'var(--font-display)', fontWeight: 800, fontSize: 22, color: 'var(--green-light)' }}>🌾 FarmMarket</div>
          <div style={{ fontSize: 11, color: 'rgba(255,255,255,0.4)', marginTop: 4 }}>
            {user?.email}
          </div>
        </div>
        <nav className="sidebar-nav" style={{ marginTop: 16 }}>
          {links.map(l => (
            <NavLink
              key={l.to}
              to={l.to}
              className={({ isActive }) => `sidebar-link${isActive ? ' active' : ''}`}
              onClick={onClose}
            >
              {l.label}
            </NavLink>
          ))}
        </nav>
      </aside>
    </>
  );
}

// ── Protected Route ───────────────────────────
export function ProtectedRoute({ children, role }) {
  const { user, loading } = useAuth();

  if (loading) return <div className="spinner" />;
  if (!user) return <Navigate to="/" replace />;
  if (role && user.role !== role) return <Navigate to="/home" replace />;

  return children;
}

// ── MacDonald AI Widget ───────────────────────
export function MacDonald() {
  const [open, setOpen] = useState(false);
  const { user } = useAuth();
  const navigate = useNavigate();

  const options = [
    ...(user?.role === 'buyer'
      ? [{ label: '🔍 Search for a product', action: () => { navigate('/marketplace'); setOpen(false); } }]
      : []),
    { label: '📦 Look up my order status', action: () => { navigate(user?.role === 'buyer' ? '/orders' : '/incoming'); setOpen(false); } },
    { label: '⭐ Leave a review', action: () => { navigate('/reviews'); setOpen(false); } },
    ...(user?.role === 'buyer'
      ? [{ label: '🛒 View my cart', action: () => { navigate('/cart'); setOpen(false); } }]
      : []),
  ];

  return (
    <div className="macdonald-widget">
      {open && (
        <div className="macdonald-popup">
          <h4>👨‍🌾 Hi! I'm MacDonald, your AI Assistant. How can I help?</h4>
          {options.map(o => (
            <button key={o.label} className="macdonald-option" onClick={o.action}>
              {o.label}
            </button>
          ))}
        </div>
      )}
      <button className="macdonald-btn" onClick={() => setOpen(p => !p)}>
        💬
      </button>
    </div>
  );
}

// ── App Shell (Navbar + Sidebar + MacDonald) ──
export function AppShell({ children }) {
  const [sidebarOpen, setSidebarOpen] = useState(false);
  return (
    <div className="app-shell">
      <Navbar onMenuClick={() => setSidebarOpen(true)} />
      <Sidebar open={sidebarOpen} onClose={() => setSidebarOpen(false)} />
      <main style={{ flex: 1, padding: '32px', maxWidth: 1200, margin: '0 auto', width: '100%' }}>
        {children}
      </main>
      <MacDonald />
    </div>
  );
}

// ── Star Rating ───────────────────────────────
export function StarRating({ value, onChange, readOnly = false }) {
  const [hover, setHover] = useState(0);
  return (
    <div className="stars">
      {[1, 2, 3, 4, 5].map(n => (
        <span
          key={n}
          className={`star${(hover || value) >= n ? ' filled' : ''}`}
          onClick={() => !readOnly && onChange && onChange(n)}
          onMouseEnter={() => !readOnly && setHover(n)}
          onMouseLeave={() => !readOnly && setHover(0)}
          style={{ cursor: readOnly ? 'default' : 'pointer' }}
        >
          ★
        </span>
      ))}
    </div>
  );
}

// ── Status Badge ──────────────────────────────
export function StatusBadge({ status }) {
  const labels = {
    pending:          'Pending',
    confirmed:        'Confirmed',
    ready_for_pickup: 'Ready for Pickup',
    completed:        'Completed',
    rejected:         'Rejected',
    cancelled:        'Cancelled',
  };
  return (
    <span className={`badge badge-${status}`}>
      {labels[status] || status}
    </span>
  );
}
