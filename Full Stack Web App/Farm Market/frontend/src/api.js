const BASE = '/api';

function getToken() {
  return localStorage.getItem('fm_token');
}

async function request(method, path, body = null, isFormData = false) {
  const headers = {};
  const token = getToken();
  if (token) headers['Authorization'] = `Bearer ${token}`;
  if (!isFormData && body) headers['Content-Type'] = 'application/json';

  const res = await fetch(`${BASE}${path}`, {
    method,
    headers,
    body: isFormData ? body : body ? JSON.stringify(body) : undefined,
  });

  const data = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(data.error || `Request failed: ${res.status}`);
  return data;
}

const get  = (path)         => request('GET',    path);
const post = (path, body, isFormData) => request('POST', path, body, isFormData);
const put  = (path, body, isFormData) => request('PUT',  path, body, isFormData);
const del  = (path)         => request('DELETE', path);

// ── Auth ──────────────────────────────────────
export const authApi = {
  register: (data) => post('/auth/register', data),
  login:    (data) => post('/auth/login', data),
  me:       ()     => get('/auth/me'),
  oauthStartUrl: (role) => `http://localhost:5000/api/auth/oauth/auth0/start?role=${encodeURIComponent(role)}`,
};

// ── Profile ───────────────────────────────────
export const profileApi = {
  getBuyer:        ()     => get('/profile/buyer'),
  updateBuyer:     (data) => put('/profile/buyer', data),
  getSeller:       ()     => get('/profile/seller'),
  updateSeller:    (data) => put('/profile/seller', data),
  getPublicFarm:   (id)   => get(`/profile/farm/${id}`),
};

// ── Products ──────────────────────────────────
export const productsApi = {
  list:   (params = {}) => {
    const qs = new URLSearchParams(params).toString();
    return get(`/products${qs ? '?' + qs : ''}`);
  },
  get:    (id)          => get(`/products/${id}`),
  mine:   (params = {}) => {
    const qs = new URLSearchParams(params).toString();
    return get(`/products/mine${qs ? '?' + qs : ''}`);
  },
  create: (formData)    => post('/products', formData, true),
  update: (id, formData) => put(`/products/${id}`, formData, true),
  delete: (id)          => del(`/products/${id}`),
};

// ── Cart ──────────────────────────────────────
export const cartApi = {
  get:    ()            => get('/cart'),
  add:    (data)        => post('/cart', data),
  update: (itemId, qty) => put(`/cart/${itemId}`, { quantity: qty }),
  remove: (itemId)      => del(`/cart/${itemId}`),
  clear:  ()            => del('/cart/clear'),
};

// ── Orders ────────────────────────────────────
export const ordersApi = {
  place:    ()               => post('/orders', {}),
  mine:     (params = {})    => {
    const qs = new URLSearchParams(params).toString();
    return get(`/orders/mine${qs ? '?' + qs : ''}`);
  },
  incoming: (params = {})    => {
    const qs = new URLSearchParams(params).toString();
    return get(`/orders/incoming${qs ? '?' + qs : ''}`);
  },
  get:      (id)             => get(`/orders/${id}`),
  cancel:   (id)             => post(`/orders/${id}/cancel`),
  approve:  (id)             => post(`/orders/${id}/approve`),
  reject:   (id)             => post(`/orders/${id}/reject`),
  markReady:(id)             => post(`/orders/${id}/ready`),
  complete: (id)             => post(`/orders/${id}/complete`),
};

// ── Reviews ───────────────────────────────────
export const reviewsApi = {
  create:       (data)     => post('/reviews', data),
  bySeller:     (sellerId) => get(`/reviews/seller/${sellerId}`),
  mine:         ()         => get('/reviews/mine'),
};

// ── Membership ────────────────────────────────
export const membershipApi = {
  get:     ()     => get('/membership'),
  upgrade: (plan) => post('/membership/upgrade', { plan }),
  plans:   ()     => get('/membership/plans'),
};
