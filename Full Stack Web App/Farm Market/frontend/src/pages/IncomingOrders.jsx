import React, { useState, useEffect } from 'react';
import { ordersApi } from '../api';
import { AppShell, StatusBadge } from '../components';

const STATUS_TABS = [
  { key: 'pending',          label: '⏳ Pending',     color: '#856404' },
  { key: 'confirmed',        label: '✅ Confirmed',   color: '#0a3622' },
  { key: 'ready_for_pickup', label: '📍 Ready',       color: '#0a367a' },
  { key: 'completed',        label: '🏁 Completed',   color: '#155724' },
  { key: 'rejected',         label: '❌ Rejected',    color: '#721c24' },
  { key: 'cancelled',        label: '🚫 Cancelled',   color: '#6c757d' },
];

function OrderCard({ order, onApprove, onReject, onReady, onComplete }) {
  const [expanded, setExpanded] = useState(false);
  const [loading, setLoading] = useState(false);

  const act = async (fn) => {
    setLoading(true);
    try { await fn(); }
    finally { setLoading(false); }
  };

  return (
    <div className="order-card" style={{ marginBottom: 12 }}>
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', flexWrap: 'wrap', gap: 12 }}>
        <div>
          <div style={{ display: 'flex', alignItems: 'center', gap: 10, marginBottom: 4 }}>
            <span style={{ fontFamily: 'var(--font-display)', fontWeight: 700, fontSize: 16 }}>Order #{order.id}</span>
            <StatusBadge status={order.status} />
          </div>
          <div style={{ fontSize: 13, color: 'var(--gray-600)' }}>
            Buyer: <strong>{order.buyer_name || 'Anonymous'}</strong>
            {' · '}
            {order.created_at
              ? new Date(order.created_at).toLocaleDateString('en-US', {
                month: 'short',
                day: 'numeric',
                year: 'numeric',
                hour: '2-digit',
                minute: '2-digit',
              })
            : 'Unknown date'}
          </div>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}>
          <span style={{ fontFamily: 'var(--font-display)', fontWeight: 800, fontSize: 22, color: 'var(--green-dark)' }}>
            ${order.total.toFixed(2)}
          </span>
          <button className="btn btn-outline btn-sm" onClick={() => setExpanded(p => !p)}>
            {expanded ? '▲ Hide' : '▼ Details'}
          </button>
        </div>
      </div>

      {expanded && (
        <div style={{ marginTop: 16, borderTop: '1px solid var(--gray-100)', paddingTop: 16 }}>
          {order.pickup_address && (
            <div style={{ marginBottom: 12, fontSize: 13, color: 'var(--gray-600)' }}>
              Pickup Address: <strong>{order.pickup_address}</strong>
            </div>
          )}

          {/* Items */}
          <div style={{ display: 'flex', flexDirection: 'column', gap: 10, marginBottom: 20 }}>
            {(order.items || []).map(item => (
              <div key={item.id} style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', fontSize: 14 }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}>
                  {item.product_image_url ? (
                    <img src={item.product_image_url} alt={item.product_name} style={{ width: 44, height: 44, borderRadius: 8, objectFit: 'cover' }} />
                  ) : (
                    <div style={{ width: 44, height: 44, borderRadius: 8, background: 'var(--gray-100)', display: 'flex', alignItems: 'center', justifyContent: 'center', fontSize: 20 }}>🌿</div>
                  )}
                  <div>
                    <div style={{ fontWeight: 600 }}>{item.product_name}</div>
                    <div style={{ color: 'var(--gray-600)', fontSize: 12 }}>Qty: {item.quantity} × ${item.price_at_purchase.toFixed(2)} / {item.price_unit || 'piece'} </div>
                  </div>
                </div>
                <span style={{ fontWeight: 600 }}>${item.subtotal.toFixed(2)}</span>
              </div>
            ))}
          </div>

          {/* Action buttons by status */}
          <div style={{ display: 'flex', gap: 10, flexWrap: 'wrap' }}>
            {order.status === 'pending' && (
              <>
                <button className="btn btn-green btn-sm" disabled={loading} onClick={() => act(onApprove)}>
                  {loading ? 'Working...' : '✓ Approve Order'}
                </button>
                <button className="btn btn-danger btn-sm" disabled={loading} onClick={() => act(onReject)}>
                  {loading ? 'Working...' : '✗ Reject Order'}  
                </button>
              </>
            )}
            {order.status === 'confirmed' && (
              <button className="btn btn-primary btn-sm" disabled={loading} onClick={() => act(onReady)}>
                {loading ? 'Working...' : '📍 Mark Ready for Pickup'}
              </button>
            )}
            {order.status === 'ready_for_pickup' && (
              <button className="btn btn-green btn-sm" disabled={loading} onClick={() => act(onComplete)}>
                {loading ? 'Working...' : '🏁 Mark Completed'}
              </button>
            )}
          </div>
        </div>
      )}
    </div>
  );
}

export default function IncomingOrders() {
  const [orders, setOrders] = useState([]);
  const [loading, setLoading] = useState(true);
  const [activeTab, setActiveTab] = useState('pending');
  const [msg, setMsg] = useState('');
  const [error, setError] = useState('');

  const fetchOrders = async () => {
    setLoading(true);
    try {
      setError('');
      const data = await ordersApi.incoming({ status: activeTab });
      setOrders(Array.isArray(data) ? data : []);
    } catch (e) {
      setError(e.message);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => { fetchOrders(); }, [activeTab]);

  const showMsg = (m) => { setMsg(m); setTimeout(() => setMsg(''), 3000); };

  const makeHandler = (fn, successMsg) => (orderId) => async () => {
    try {
      setError('');
      await fn(orderId);
      showMsg(successMsg);
      await fetchOrders();
    } catch (e) {
      setError(e.message);
    }
  };

  const handleApprove  = makeHandler(ordersApi.approve,   '✅ Order approved! Inventory deducted.');
  const handleReject   = makeHandler(ordersApi.reject,    '❌ Order rejected. Inventory released.');
  const handleReady    = makeHandler(ordersApi.markReady, '📍 Order marked ready for pickup!');
  const handleComplete = makeHandler(ordersApi.complete,  '🏁 Order completed!');

  return (
    <AppShell>
      <div className="section-row">
        <div>
          <h1 className="page-title">📬 Incoming Orders</h1>
          <p className="page-subtitle">Manage orders from your buyers</p>
        </div>
      </div>

      {error && <div className="alert alert-error">{error}</div>}
      {msg   && <div className="alert alert-success">{msg}</div>}

      {/* Status tabs */}
      <div style={{ display: 'flex', gap: 0, marginBottom: 24, borderBottom: '2px solid var(--gray-200)', overflowX: 'auto' }}>
        {STATUS_TABS.map(tab => (
          <button
            key={tab.key}
            onClick={() => setActiveTab(tab.key)}
            style={{
              padding: '10px 20px',
              border: 'none',
              background: 'none',
              fontFamily: 'var(--font-body)',
              fontWeight: activeTab === tab.key ? 600 : 400,
              fontSize: 14,
              color: activeTab === tab.key ? tab.color : 'var(--gray-600)',
              borderBottom: activeTab === tab.key ? `3px solid ${tab.color}` : '3px solid transparent',
              marginBottom: -2,
              cursor: 'pointer',
              transition: 'all 0.15s',
              whiteSpace: 'nowrap',
            }}
          >
            {tab.label}
          </button>
        ))}
      </div>

      {loading ? (
        <div className="spinner" />
      ) : orders.length === 0 ? (
        <div style={{ textAlign: 'center', padding: 56, color: 'var(--gray-600)' }}>
          <div style={{ fontSize: 48, marginBottom: 12 }}>📭</div>
          <p>No {activeTab.replace(/_/g, ' ')} orders.</p>
        </div>
      ) : (
        orders.map(order => (
          <OrderCard
            key={order.id}
            order={order}
            onApprove={handleApprove(order.id)}
            onReject={handleReject(order.id)}
            onReady={handleReady(order.id)}
            onComplete={handleComplete(order.id)}
          />
        ))
      )}
    </AppShell>
  );
}
