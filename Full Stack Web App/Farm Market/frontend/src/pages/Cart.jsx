import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { cartApi, ordersApi } from '../api';
import { AppShell } from '../components';

export default function Cart() {
  const navigate = useNavigate();
  const [cart, setCart]     = useState({ items: [], total: 0 });
  const [loading, setLoading] = useState(true);
  const [placing, setPlacing] = useState(false);
  const [error, setError]   = useState('');
  const [success, setSuccess] = useState('');
  const [draftQtys, setDraftQtys] = useState({});

  const fetchCart = async () => {
    try {
      setError('');
      const data = await cartApi.get();
      setCart(data || { items: [], total: 0 });
    } catch (e) {
      setError(e.message);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => { fetchCart(); }, []);

  useEffect(() => {
    const next = {};
    (cart.items || []).forEach(item => {
      next[item.id] = String(item.quantity);
    });
    setDraftQtys(next);
  }, [cart.items])

  const updateQty = async (itemId, qty) => {
    try {
      setError('');

      if (qty <= 0) {
        await removeItem(itemId);
        return;
      }

      await cartApi.update(itemId, qty);
      await fetchCart();
    } catch (e) {
      setError(e.message);
    }
  };

  const removeItem = async (itemId) => {
    try {
      setError('');
      await cartApi.remove(itemId);
      await fetchCart();
    } catch (e) {
      setError(e.message);
    }
  };

  const submitTypedQty = async (item) => {
    const raw = draftQtys[item.id];
    const parsed = parseInt(raw, 10);
    const available = item.product?.available_qty ?? 0;

    setError('');

    if (!raw || Number.isNaN(parsed)) {
      setError('Enter a valid quantity.');
      setDraftQtys(prev => ({ ...prev, [item.id]: String(item.quantity) }));
      return;
    }

    if (parsed < 1) {
      setError('Quantity must be at least 1.');
      setDraftQtys(prev => ({ ...prev, [item.id]: String(item.quantity) }));
      return;
    }

    if (parsed > available) {
      setError(`Only ${available} units available.`);
      setDraftQtys(prev => ({ ...prev, [item.id]: String(item.quantity) }));
      return;
    }

    await updateQty(item.id, parsed);
  };

  const checkout = async () => {
    setPlacing(true);
    setError('');
    setSuccess('');

    try {
      await ordersApi.place();
      setCart({ items: [], total: 0 });
      setSuccess('Order placed! 🎉 Waiting for farmer approval.');
      setTimeout(() => navigate('/orders'), 2000);
    } catch (e) {
      setError(e.message);
    } finally {
      setPlacing(false);
    }
  };

  return (
    <AppShell>
      <div style={{ maxWidth: 860, margin: '0 auto' }}>
        <div className="section-row">
          <h1 className="page-title">🛒 Shopping Cart</h1>
          {cart.items.length > 0 && (
            <button
              className="btn btn-green btn-lg"
              onClick={checkout}
              disabled={placing}
            >
              {placing ? 'Placing order...' : `Checkout · $${cart.total.toFixed(2)}`}
            </button>
          )}
        </div>

        {error   && <div className="alert alert-error">{error}</div>}
        {success && <div className="alert alert-success">{success}</div>}

        {loading ? (
          <div className="spinner" />
        ) : cart.items.length === 0 ? (
          <div style={{ textAlign: 'center', padding: 64, color: 'var(--gray-600)' }}>
            <div style={{ fontSize: 56, marginBottom: 16 }}>🛒</div>
            <h3 style={{ fontFamily: 'var(--font-display)', fontWeight: 700, marginBottom: 8 }}>Your cart is empty</h3>
            <p style={{ marginBottom: 24 }}>Browse fresh produce from local farms</p>
            <button className="btn btn-green" onClick={() => navigate('/marketplace')}>
              Browse Marketplace
            </button>
          </div>
        ) : (
          <div style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
            {cart.items.map(item => (
              <div key={item.id} className="card" style={{ display: 'flex', alignItems: 'center', gap: 20, padding: 20 }}>
                {/* Image */}
                <div style={{
                  width: 90, height: 90, borderRadius: 'var(--radius-md)',
                  background: 'var(--gray-100)', overflow: 'hidden', flexShrink: 0,
                  display: 'flex', alignItems: 'center', justifyContent: 'center', fontSize: 36,
                }}>
                  {item.product?.image_url
                    ? <img src={item.product.image_url} alt={item.product.name} style={{ width: '100%', height: '100%', objectFit: 'cover' }} />
                    : '🌿'}
                </div>

                {/* Info */}
                <div style={{ flex: 1 }}>
                  <div style={{ fontWeight: 600, fontSize: 16, marginBottom: 4 }}>{item.product?.name}</div>
                  <div style={{ color: 'var(--gray-600)', fontSize: 13, marginBottom: 8 }}>
                    ${item.product?.price?.toFixed(2)} / {item.product?.price_unit || 'piece'}
                  </div>
                  <div style={{ fontFamily: 'var(--font-display)', fontWeight: 700, fontSize: 18, color: 'var(--green-dark)' }}>
                    ${item.subtotal.toFixed(2)}
                  </div>
                </div>

                {/* Qty controls */}
                <div style={{ display: 'flex', alignItems: 'center', gap: 0 }}>
                  <button
                    onClick={() => updateQty(item.id, item.quantity - 1)}
                    style={{
                      width: 40,
                      height: 40,
                      background: '#1e1e20',
                      color: '#fff',
                      border: 'none',
                      fontSize: 20,
                      cursor: 'pointer',
                      borderRadius: 'var(--radius-sm) 0 0 var(--radius-sm)',
                    }}
                  >
                    -
                  </button>

                  <input
                    type="number"
                    min="1"
                    max={item.product?.available_qty ?? undefined}
                    value={draftQtys[item.id] ?? item.quantity}
                    onChange={e =>
                      setDraftQtys(prev => ({
                        ...prev,
                        [item.id]: e.target.value,
                      }))
                    }
                    onBlur={() => submitTypedQty(item)}
                    onKeyDown={e => {
                      if (e.key === 'Enter') {
                        e.preventDefault();
                        submitTypedQty(item);
                      }
                      if (e.key === 'Escape') {
                        setDraftQtys(prev => ({
                          ...prev,
                          [item.id]: String(item.quantity),
                        }));
                      }
                    }}
                    style={{
                      width: 64,
                      height: 40,
                      background: 'var(--gray-100)',
                      border: '1px solid var(--gray-200)',
                      textAlign: 'center',
                      fontFamily: 'var(--font-display)',
                      fontWeight: 700,
                      fontSize: 16,
                      outline: 'none',
                    }}
                  />

                <button
                  onClick={() => updateQty(item.id, item.quantity + 1)}
                  disabled={item.quantity >= (item.product?.available_qty ?? 0)}
                  style={{
                    width: 40,
                    height: 40,
                    background: '#1e1e20',
                    color: '#fff',
                    border: 'none',
                    fontSize: 20,
                    cursor: item.quantity >= (item.product?.available_qty ?? 0) ? 'not-allowed' : 'pointer',
                    opacity: item.quantity >= (item.product?.available_qty ?? 0) ? 0.5 : 1,
                    borderRadius: '0 var(--radius-sm) var(--radius-sm) 0',
                  }}
                >
                  +
                </button>
              </div>

                {/* Remove */}
                <button
                  className="btn btn-danger btn-sm"
                  onClick={() => removeItem(item.id)}
                >
                  Remove
                </button>
              </div>
            ))}

            {/* Total row */}
            <div style={{
              background: '#fff', borderRadius: 'var(--radius-md)', padding: '20px 24px',
              display: 'flex', justifyContent: 'space-between', alignItems: 'center',
              boxShadow: 'var(--shadow-sm)', border: '1px solid var(--gray-200)',
              marginTop: 8,
            }}>
              <span style={{ fontFamily: 'var(--font-display)', fontWeight: 700, fontSize: 20 }}>
                Total
              </span>
              <span style={{ fontFamily: 'var(--font-display)', fontWeight: 800, fontSize: 28, color: 'var(--green-dark)' }}>
                ${cart.total.toFixed(2)}
              </span>
            </div>

            <button
              className="btn btn-green-light btn-full"
              style={{ marginTop: 8 }}
              onClick={checkout}
              disabled={placing}
            >
              {placing ? 'Placing Order...' : 'Place Order'}
            </button>
          </div>
        )}
      </div>
    </AppShell>
  );
}
