import React, { useState, useEffect } from 'react';
import { ordersApi, reviewsApi } from '../api';
import { AppShell, StatusBadge, StarRating } from '../components';

const STATUS_FILTERS = ['all', 'pending', 'confirmed', 'ready_for_pickup', 'completed', 'rejected', 'cancelled'];

function ReviewModal({ order, onClose, onSubmit }) {
  const [rating, setRating] = useState(5);
  const [comment, setComment] = useState('');
  const [submitting, setSubmitting] = useState(false);
  const [error, setError] = useState('');

  const submit = async () => {
    setSubmitting(true);
    setError('');
    try {
      await reviewsApi.create({ order_id: order.id, rating, comment });
      onSubmit();
    } catch (e) {
      setError(e.message);
    } finally {
      setSubmitting(false);
    }
  };

  return (
    <div style={{
      position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.5)',
      display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 300,
    }}>
      <div style={{ background: '#fff', borderRadius: 'var(--radius-md)', padding: 32, width: 420, boxShadow: 'var(--shadow-lg)' }}>
        <h3 style={{ fontFamily: 'var(--font-display)', fontWeight: 700, fontSize: 20, marginBottom: 8 }}>
          ⭐ Leave a Review
        </h3>
        <p style={{ color: 'var(--gray-600)', fontSize: 14, marginBottom: 20 }}>
          Reviewing order from <strong>{order.farm_name || 'Local Farm'}</strong>
        </p>
        {error && <div className="alert alert-error">{error}</div>}
        <div className="form-group" style={{ marginBottom: 16 }}>
          <label className="form-label">Rating</label>
          <StarRating value={rating} onChange={setRating} />
        </div>
        <div className="form-group" style={{ marginBottom: 24 }}>
          <label className="form-label">Comment</label>
          <textarea
            className="form-textarea"
            placeholder="How was your experience?"
            value={comment}
            onChange={e => setComment(e.target.value)}
            rows={3}
          />
        </div>
        <div style={{ display: 'flex', gap: 12, justifyContent: 'flex-end' }}>
          <button className="btn btn-outline" onClick={onClose}>Cancel</button>
          <button className="btn btn-green" onClick={submit} disabled={submitting}>
            {submitting ? 'Submitting...' : 'Submit Review'}
          </button>
        </div>
      </div>
    </div>
  );
}

function OrderCard({ order, onCancel, onComplete, onReview }) {
  const [expanded, setExpanded] = useState(false);

  return (
    <div className="order-card" style={{ marginBottom: 12 }}>
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', flexWrap: 'wrap', gap: 12 }}>
        <div>
          <div style={{ display: 'flex', alignItems: 'center', gap: 12, marginBottom: 4 }}>
            <span style={{ fontFamily: 'var(--font-display)', fontWeight: 700, fontSize: 16 }}>
              Order #{order.id}
            </span>
            <StatusBadge status={order.status} />
          </div>
          <div style={{ fontSize: 13, color: 'var(--gray-600)' }}>
            {order.farm_name || 'Unknown Farm'} · {order.created_at ? new Date(order.created_at).toLocaleDateString() : 'Unknown date'}
          </div>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}>
          <span style={{ fontFamily: 'var(--font-display)', fontWeight: 800, fontSize: 22, color: 'var(--green-dark)' }}>
            ${order.total.toFixed(2)}
          </span>
          <button className="btn btn-outline btn-sm" onClick={() => setExpanded(p => !p)}>
            {expanded ? 'Hide' : 'Details'}
          </button>
        </div>
      </div>

      {expanded && (
        <div style={{ marginTop: 16, borderTop: '1px solid var(--gray-100)', paddingTop: 16 }}>
          <div style={{ display: 'flex', flexDirection: 'column', gap: 10, marginBottom: 16 }}>
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

          <div style={{ display: 'flex', gap: 10, flexWrap: 'wrap' }}>
            {['pending', 'confirmed'].includes(order.status) && (
              <button className="btn btn-danger btn-sm" onClick={() => onCancel(order.id)}>
                Cancel Order
              </button>
            )}
            {order.status === 'ready_for_pickup' && (
              <button className="btn btn-green btn-sm" onClick={() => onComplete(order.id)}>
                Mark as Collected ✓
              </button>
            )}
            {order.status === 'completed' && !order.review && (
              <button className="btn btn-outline btn-sm" onClick={() => onReview(order)}>
                ⭐ Leave a Review
              </button>
            )}
            {order.status === 'completed' && order.review && (
              <span style={{ fontSize: 13, color: 'var(--green-mid)', fontWeight: 500 }}>
                ⭐ Reviewed ({order.review.rating}/5)
              </span>
            )}
          </div>
        </div>
      )}
    </div>
  );
}

export default function MyOrders() {
  const [orders, setOrders] = useState([]);
  const [loading, setLoading] = useState(true);
  const [statusFilter, setStatusFilter] = useState('all');
  const [reviewOrder, setReviewOrder] = useState(null);
  const [error, setError] = useState('');
  const [msg, setMsg] = useState('');

  const fetchOrders = async () => {
    setLoading(true);
    try {
      setError('');
      const params = statusFilter !== 'all' ? { status: statusFilter } : {};
      const data = await ordersApi.mine(params);
      setOrders(Array.isArray(data) ? data : []);
    } catch (e) {
      setError(e.message);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => { fetchOrders(); }, [statusFilter]);

  const handleCancel = async (id) => {
    try {
      setError('');
      setMsg('');
      await ordersApi.cancel(id);
      setMsg('Order cancelled.');
      await fetchOrders();
    } catch (e) {
      setError(e.message);
    }
  };

  const handleComplete = async (id) => {
    try {
      setError('');
      setMsg('');
      await ordersApi.complete(id);
      setMsg('Order marked as collected! ✓');
      await fetchOrders();
    } catch (e) {
      setError(e.message);
    }
  };

  return (
    <AppShell>
      <div className="section-row">
        <div>
          <h1 className="page-title">📦 My Orders</h1>
          <p className="page-subtitle">Track your purchases from local farms</p>
        </div>
      </div>

      {error && <div className="alert alert-error">{error}</div>}
      {msg   && <div className="alert alert-success">{msg}</div>}

      {/* Status filter chips */}
      <div className="filter-chips" style={{ marginBottom: 24 }}>
        {STATUS_FILTERS.map(s => (
          <button key={s} className={`chip${statusFilter === s ? ' active' : ''}`} onClick={() => setStatusFilter(s)}>
            {s === 'all' ? 'All Orders' : s.replace(/_/g, ' ')}
          </button>
        ))}
      </div>

      {loading ? (
        <div className="spinner" />
      ) : orders.length === 0 ? (
        <div style={{ textAlign: 'center', padding: 56, color: 'var(--gray-600)' }}>
          <div style={{ fontSize: 48, marginBottom: 12 }}>📭</div>
          <p>No orders found.</p>
        </div>
      ) : (
        orders.map(order => (
          <OrderCard
            key={order.id}
            order={order}
            onCancel={handleCancel}
            onComplete={handleComplete}
            onReview={setReviewOrder}
          />
        ))
      )}

      {reviewOrder && (
        <ReviewModal
          order={reviewOrder}
          onClose={() => setReviewOrder(null)}
          onSubmit={async () => {
            setReviewOrder(null);
            setError('');
            setMsg('Review submitted! ⭐');
            await fetchOrders();
          }}
        />
      )}
    </AppShell>
  );
}
