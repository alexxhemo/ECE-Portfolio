import React, { useState, useEffect, useCallback } from 'react';
import { useNavigate } from 'react-router-dom';
import { productsApi, cartApi } from '../api';
import { AppShell } from '../components';
import { useAuth } from '../context/AuthContext';

const CATEGORIES = ['All', 'Produce', 'Vegetables', 'Dairy', 'Meat', 'Baked Goods', 'Fruit'];
const EMOJI = { Produce: '🥬', Vegetables: '🥕', Dairy: '🥛', Meat: '🥩', 'Baked Goods': '🍞', Fruit: '🍓', Other: '🌿' };

export default function Marketplace() {
  const { user } = useAuth();
  const navigate = useNavigate();
  const [products, setProducts] = useState([]);
  const [loading, setLoading] = useState(true);
  const [category, setCategory] = useState('All');
  const [search, setSearch] = useState('');
  const [zipFilter, setZipFilter] = useState('');
  const [zipInput, setZipInput] = useState('');
  const [cartMsg, setCartMsg] = useState('');

  const fetchProducts = useCallback(async () => {
    setLoading(true);
    try {
      const params = {};
      if (category !== 'All') params.category = category;
      if (search) params.search = search;
      if (zipFilter) params.zip_code = zipFilter;
      const data = await productsApi.list(params);
      setProducts(data);
    } catch (e) {
      console.error(e);
    } finally {
      setLoading(false);
    }
  }, [category, search, zipFilter]);

  useEffect(() => { fetchProducts(); }, [fetchProducts]);

  const addToCart = async (e, productId) => {
    e.stopPropagation();
    if (!user || user.role !== 'buyer') { navigate('/buyer/login'); return; }
    try {
      await cartApi.add({ product_id: productId, quantity: 1 });
      setCartMsg('Added to cart! 🛒');
      setTimeout(() => setCartMsg(''), 2500);
    } catch (err) {
      setCartMsg(err.message);
      setTimeout(() => setCartMsg(''), 2500);
    }
  };

  return (
    <AppShell>
      <div className="section-row">
        <div>
          <h1 className="page-title">🏪 Marketplace</h1>
          <p className="page-subtitle">Fresh from local farms near you</p>
        </div>
      </div>

      {/* Toast */}
      {cartMsg && (
        <div className={`alert ${cartMsg.startsWith('Added') ? 'alert-success' : 'alert-error'}`} style={{ position: 'fixed', top: 70, right: 24, zIndex: 300, width: 260 }}>
          {cartMsg}
        </div>
      )}

      {/* Search + Filters */}
      <div style={{ display: 'flex', gap: 16, marginBottom: 20, flexWrap: 'wrap', alignItems: 'center' }}>
        <div className="search-bar">
          <span className="search-icon">🔍</span>
          <input
            placeholder="Search products..."
            value={search}
            onChange={e => setSearch(e.target.value)}
          />
        </div>
        <div style={{ display: 'flex', gap: 8, alignItems: 'center' }}>
          <input
            className="form-input"
            placeholder="Filter by zip..."
            value={zipInput}
            onChange={e => setZipInput(e.target.value)}
            style={{ width: 140, padding: '8px 12px', fontSize: 13 }}
          />
          <button
            className="btn btn-green btn-sm"
            onClick={() => setZipFilter(zipInput)}
          >
            Filter by Location
          </button>
          {zipFilter && (
            <button className="btn btn-outline btn-sm" onClick={() => { setZipFilter(''); setZipInput(''); }}>
              Clear
            </button>
          )}
        </div>
      </div>

      {/* Category chips */}
      <div className="filter-chips" style={{ marginBottom: 24 }}>
        {CATEGORIES.map(c => (
          <button
            key={c}
            className={`chip${category === c ? ' active' : ''}`}
            onClick={() => setCategory(c)}
          >
            {EMOJI[c] || '🌿'} {c}
          </button>
        ))}
      </div>

      {/* Grid */}
      {loading ? (
        <div className="spinner" />
      ) : products.length === 0 ? (
        <div style={{ textAlign: 'center', padding: 48, color: 'var(--gray-600)' }}>
          <div style={{ fontSize: 48, marginBottom: 12 }}>🌱</div>
          <p>No products found. Try different filters.</p>
        </div>
      ) : (
        <div className="product-grid">
          {products.map(p => (
            <div key={p.id} className="product-card">
              {p.image_url ? (
                <img src={p.image_url} alt={p.name} className="product-card-img" />
              ) : (
                <div className="product-card-img-placeholder">
                  {EMOJI[p.category] || '🌿'}
                </div>
              )}
              <div className="product-card-body">
                <div className="product-card-name">{p.name}</div>
                <div className="product-card-farm">{p.farm_name || 'Local Farm'}</div>
                {p.zip_code && (
                  <div style={{ fontSize: 12, color: 'var(--gray-600)', marginTop: 2 }}>
                    📍 {p.zip_code}
                  </div>
                )}
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <span className="product-card-price">${p.price.toFixed(2)} / {p.price_unit || 'piece'} </span>
                  <span style={{ fontSize: 11, color: 'var(--gray-600)' }}>
                    {p.available_qty} left
                  </span>
                </div>
                <span className="product-card-category">{p.category}</span>
                {user?.role === 'buyer' && (
                  <button
                    className="btn btn-green btn-sm btn-full"
                    style={{ marginTop: 10 }}
                    onClick={e => addToCart(e, p.id)}
                  >
                    + Add to Cart
                  </button>
                )}
              </div>
            </div>
          ))}
        </div>
      )}
    </AppShell>
  );
}
