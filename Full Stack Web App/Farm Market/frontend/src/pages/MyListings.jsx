import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { productsApi } from '../api';
import { AppShell } from '../components';

const CATEGORIES = ['Produce', 'Vegetables', 'Dairy', 'Meat', 'Baked Goods', 'Fruit', 'Other'];
const EMOJI = { Produce: '🥬', Vegetables: '🥕', Dairy: '🥛', Meat: '🥩', 'Baked Goods': '🍞', Fruit: '🍓', Other: '🌿' };
const PRICE_UNITS = ['piece', 'pound', 'gallon', 'dozen', 'bag', 'bunch'];

// ── Create / Edit Listing Form ─────────────────
export function CreateListing() {
  const navigate = useNavigate();
  const [form, setForm] = useState({
    name: '', description: '', price: '', quantity: '', category: 'Produce', price_unit: 'piece'
  });
  const [image, setImage] = useState(null);
  const [preview, setPreview] = useState('');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');

  const set = k => e => setForm(f => ({ ...f, [k]: e.target.value }));

  const handleImage = e => {
    const file = e.target.files[0];
    if (!file) return;
    setImage(file);
    setPreview(URL.createObjectURL(file));
  };

  const handleSubmit = async e => {
    e.preventDefault();
    setLoading(true); setError('');
    try {
      const fd = new FormData();
      fd.append('name',        form.name);
      fd.append('description', form.description);
      fd.append('price',       form.price);
      fd.append('quantity',    form.quantity);
      fd.append('category',    form.category);
      fd.append('price_unit', form.price_unit);
      if (image) fd.append('image', image);
      await productsApi.create(fd);
      navigate('/listings');
    } catch (e) {
      setError(e.message);
    } finally {
      setLoading(false);
    }
  };

  return (
    <AppShell>
      <div style={{ maxWidth: 560, margin: '0 auto' }}>
        <h1 className="page-title" style={{ marginBottom: 4 }}>Create Listing</h1>
        <p className="page-subtitle">Add a new product to the marketplace</p>
        <div style={{ borderBottom: '1px solid var(--gray-200)', marginBottom: 24 }} />

        {error && <div className="alert alert-error">{error}</div>}

        <form onSubmit={handleSubmit} style={{ display: 'flex', flexDirection: 'column', gap: 20 }}>
          <div className="form-group">
            <label className="form-label">Product Name *</label>
            <input className="form-input" placeholder="Enter Product Name" required value={form.name} onChange={set('name')} />
          </div>

          <div className="form-group">
            <label className="form-label">Product Category</label>
            <select className="form-select" value={form.category} onChange={set('category')}>
              {CATEGORIES.map(c => <option key={c} value={c}>{c}</option>)}
            </select>
          </div>

          <div className="form-group">
            <label className="form-label">Product Image</label>
            <label style={{
              display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center',
              border: '2px dashed var(--gray-200)', borderRadius: 'var(--radius-md)',
              padding: 24, cursor: 'pointer', background: 'var(--gray-50)', gap: 8,
              minHeight: 120, position: 'relative', overflow: 'hidden',
            }}>
              {preview ? (
                <img src={preview} alt="preview" style={{ width: '100%', height: 160, objectFit: 'cover', borderRadius: 8 }} />
              ) : (
                <>
                  <span style={{ fontSize: 32 }}>☁️</span>
                  <span style={{ fontWeight: 600, color: 'var(--green-mid)' }}>Upload a File</span>
                  <span style={{ fontSize: 12, color: 'var(--gray-600)' }}>Drag and drop files here</span>
                </>
              )}
              <input type="file" accept="image/*" onChange={handleImage} style={{ display: 'none' }} />
            </label>
          </div>

          <div className="form-group">
            <label className="form-label">Product Description</label>
            <textarea className="form-textarea" placeholder="Describe your product..." value={form.description} onChange={set('description')} rows={4} />
          </div>

          <div style={{ display: 'flex', gap: 16 }}>
            <div className="form-group" style={{ flex: 1 }}>
              <label className="form-label">Price ($) *</label>
              <input
                className="form-input"
                type="number"
                step="0.01"
                min="0"
                placeholder="0.00"
                required
                value={form.price}
                onChange={set('price')}
              />
            </div>

            <div className="form-group" style={{ flex: 1 }}>
              <label className="form-label">Price Unit</label>
              <select
                className="form-select"
                value={form.price_unit}
                onChange={set('price_unit')}
              >
                {PRICE_UNITS.map(unit => (
                  <option key={unit} value={unit}>
                    {unit}
                  </option>
                ))}
              </select>
            </div>

            <div className="form-group" style={{ flex: 1 }}>
              <label className="form-label">Amount in Stock *</label>
              <input
                className="form-input"
                type="number"
                min="0"
                placeholder="0"
                required
                value={form.quantity}
                onChange={set('quantity')}
              />
            </div>
          </div>

          <button className="btn btn-green-light btn-full" type="submit" disabled={loading}>
            {loading ? 'Creating...' : 'Submit Listing'}
          </button>
          <button type="button" className="btn btn-outline btn-full" onClick={() => navigate('/listings')}>
            Cancel
          </button>
        </form>
      </div>
    </AppShell>
  );
}

// ── Edit Listing Form ──────────────────────────
export function EditListing({ productId, onSave, onCancel }) {
  const [form, setForm] = useState(null);
  const [image, setImage] = useState(null);
  const [preview, setPreview] = useState('');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');

  useEffect(() => {
    const loadProduct = async () => {
      try {
        setError('');
        const p = await productsApi.get(productId);
        setForm({
          name: p.name || '',
          description: p.description || '',
          price: p.price ?? '',
          quantity: p.quantity ?? '',
          category: p.category || 'Produce',
          price_unit: p.price_unit || 'piece',
        });
        setPreview(p.image_url || '');
      } catch (e) {
        setError(e.message);
      }
    };

  loadProduct();
}, [productId]);

  const set = k => e => setForm(f => ({ ...f, [k]: e.target.value }));

  const handleImage = e => {
    const file = e.target.files[0];
    if (!file) return;
    setImage(file);
    setPreview(URL.createObjectURL(file));
  };

  const handleSubmit = async e => {
    e.preventDefault();
    setLoading(true); setError('');
    try {
      const fd = new FormData();
      Object.entries(form).forEach(([k, v]) => fd.append(k, v));
      if (image) fd.append('image', image);
      await productsApi.update(productId, fd);
      onSave();
    } catch (e) {
      setError(e.message);
    } finally {
      setLoading(false);
    }
  };

  if (!form) return <div className="spinner" />;

  return (
    <div style={{ background: '#fff', borderRadius: 'var(--radius-md)', padding: 28, boxShadow: 'var(--shadow-lg)', border: '1px solid var(--gray-200)' }}>
      <h3 style={{ fontFamily: 'var(--font-display)', fontWeight: 700, marginBottom: 20 }}>Edit Listing</h3>
      {error && <div className="alert alert-error">{error}</div>}
      <form onSubmit={handleSubmit} style={{ display: 'flex', flexDirection: 'column', gap: 16 }}>
        <div className="form-group">
          <label className="form-label">Product Name</label>
          <input className="form-input" value={form.name} onChange={set('name')} />
        </div>
        <div className="form-group">
          <label className="form-label">Category</label>
          <select className="form-select" value={form.category} onChange={set('category')}>
            {CATEGORIES.map(c => <option key={c} value={c}>{c}</option>)}
          </select>
        </div>
        <div className="form-group">
          <label className="form-label">Image</label>
          <label style={{ border: '2px dashed var(--gray-200)', borderRadius: 8, padding: 12, cursor: 'pointer', display: 'block', textAlign: 'center' }}>
            {preview ? <img src={preview} alt="preview" style={{ maxHeight: 100, objectFit: 'cover', borderRadius: 6 }} /> : <span style={{ color: 'var(--gray-600)' }}>Click to upload image</span>}
            <input type="file" accept="image/*" onChange={handleImage} style={{ display: 'none' }} />
          </label>
        </div>
        <div className="form-group">
          <label className="form-label">Description</label>
          <textarea className="form-textarea" rows={2} value={form.description} onChange={set('description')} />
        </div>
        <div style={{ display: 'flex', gap: 12 }}>
          <div className="form-group" style={{ flex: 1 }}>
            <label className="form-label">Price ($)</label>
            <input
              className="form-input"
              type="number"
              step="0.01"
              value={form.price}
              onChange={set('price')}
            />
          </div>

          <div className="form-group" style={{ flex: 1 }}>
            <label className="form-label">Price Unit</label>
            <select
              className="form-select"
              value={form.price_unit}
              onChange={set('price_unit')}
            >
              {PRICE_UNITS.map(unit => (
                <option key={unit} value={unit}>
                  {unit}
                </option>
              ))}
            </select>
          </div>

          <div className="form-group" style={{ flex: 1 }}>
            <label className="form-label">Stock</label>
            <input
              className="form-input"
              type="number"
              value={form.quantity}
              onChange={set('quantity')}
            />
          </div>
        </div>
        <div style={{ display: 'flex', gap: 10 }}>
          <button className="btn btn-outline" type="button" onClick={onCancel}>Cancel</button>
          <button className="btn btn-green" type="submit" disabled={loading}>{loading ? 'Saving...' : 'Save Changes'}</button>
        </div>
      </form>
    </div>
  );
}

// ── My Listings Page ───────────────────────────
export default function MyListings() {
  const navigate = useNavigate();
  const [products, setProducts] = useState([]);
  const [loading, setLoading] = useState(true);
  const [stockFilter, setStockFilter] = useState('');
  const [editingId, setEditingId] = useState(null);
  const [error, setError] = useState('');
  const [msg, setMsg] = useState('');

  const fetchProducts = async () => {
    setLoading(true);
    try {
      setError('');
      const params = stockFilter ? { status: stockFilter } : {};
      const data = await productsApi.mine(params);
      setProducts(Array.isArray(data) ? data : []);
    } catch (e) {
      setError(e.message);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => { fetchProducts(); }, [stockFilter]);

  const handleDelete = async (id) => {
    if (!window.confirm('Delete this listing?')) return;
    try {
      setError('');
      setMsg('');
      await productsApi.delete(id);
      setMsg('Listing deleted.');
      await fetchProducts();
    } catch (e) {
      setError(e.message);
    }
  };

  return (
    <AppShell>
      <div className="section-row">
        <div>
          <h1 className="page-title">🏪 My Listings</h1>
          <p className="page-subtitle">Manage your products on the marketplace</p>
        </div>
        <button className="btn btn-green" onClick={() => navigate('/listings/new')}>
          + Create New Listing
        </button>
      </div>

      {error && <div className="alert alert-error">{error}</div>}
      {msg   && <div className="alert alert-success">{msg}</div>}

      {/* Stock filters */}
      <div className="filter-chips" style={{ marginBottom: 24 }}>
        {[['', 'All'], ['in_stock', 'In Stock'], ['out_of_stock', 'Out of Stock']].map(([val, label]) => (
          <button key={val} className={`chip${stockFilter === val ? ' active' : ''}`} onClick={() => setStockFilter(val)}>
            {label}
          </button>
        ))}
      </div>

      {loading ? <div className="spinner" /> : products.length === 0 ? (
        <div style={{ textAlign: 'center', padding: 56, color: 'var(--gray-600)' }}>
          <div style={{ fontSize: 48, marginBottom: 12 }}>🌱</div>
          <p>No listings yet. Create your first product!</p>
          <button className="btn btn-green" style={{ marginTop: 16 }} onClick={() => navigate('/listings/new')}>Create Listing</button>
        </div>
      ) : (
        <div style={{ display: 'flex', flexDirection: 'column', gap: 16 }}>
          {products.map(p => (
            <div key={p.id}>
              {editingId === p.id ? (
                <EditListing
                  productId={p.id}
                  onSave={async () => {
                    setEditingId(null);
                    setError('');
                    setMsg('Listing updated! ✓');
                    await fetchProducts();
                  }}
                  onCancel={() => {
                    setEditingId(null);
                    setError('');
                  }}
                />
              ) : (
                <div className="card" style={{ display: 'flex', gap: 20, padding: 20, alignItems: 'center' }}>
                  {/* Image */}
                  <div style={{ width: 90, height: 90, borderRadius: 'var(--radius-md)', overflow: 'hidden', flexShrink: 0, background: 'var(--gray-100)', display: 'flex', alignItems: 'center', justifyContent: 'center', fontSize: 32 }}>
                    {p.image_url ? <img src={p.image_url} alt={p.name} style={{ width: '100%', height: '100%', objectFit: 'cover' }} /> : EMOJI[p.category] || '🌿'}
                  </div>

                  {/* Info */}
                  <div style={{ flex: 1 }}>
                    <div style={{ fontWeight: 700, fontSize: 16, marginBottom: 2 }}>{p.name}</div>
                    <div style={{ fontSize: 12, color: 'var(--gray-600)', marginBottom: 6 }}>
                     Added: {p.created_at
                      ? new Date(p.created_at).toLocaleDateString('en-US', {
                          day: '2-digit',
                          month: 'short',
                          year: 'numeric',
                        })
                      : 'Unknown date'}
                    </div>
                    <div style={{ display: 'flex', gap: 16, alignItems: 'center' }}>
                      <span style={{ fontFamily: 'var(--font-display)', fontWeight: 700, fontSize: 17, color: 'var(--green-dark)' }}>
                        ${p.price.toFixed(2)} / {p.price_unit || 'piece'}
                      </span>
                      <span style={{ fontSize: 13, color: p.available_qty > 0 ? 'var(--green-mid)' : 'var(--red)' }}>
                        {p.available_qty > 0 ? `${p.available_qty} available` : 'Out of stock'}
                      </span>
                      <span className="badge badge-green">{p.category}</span>
                    </div>
                  </div>

                  {/* Actions */}
                  <div style={{ display: 'flex', gap: 8, flexShrink: 0 }}>
                    <button
                      className="btn btn-ghost btn-sm"
                      onClick={() => {
                        setError('');
                        setEditingId(p.id);
                      }}
                    >
                      EDIT
                    </button>
                    <button className="btn btn-danger btn-sm" onClick={() => handleDelete(p.id)}>DELETE</button>
                  </div>
                </div>
              )}
            </div>
          ))}
        </div>
      )}
    </AppShell>
  );
}
