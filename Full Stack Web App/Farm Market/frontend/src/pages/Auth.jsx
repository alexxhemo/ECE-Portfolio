import React, { useState } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { authApi } from '../api';

function OAuthButton({ role }) {
  const label = role === 'seller'
    ? 'Continue with Auth0 as Seller'
    : 'Continue with Auth0 as Buyer';

  return (
    <button
      className="btn btn-outline btn-lg btn-full"
      type="button"
      onClick={() => { window.location.href = authApi.oauthStartUrl(role); }}
    >
      {label}
    </button>
  );
}

function AuthLayout({ children }) {
  return (
    <div className="auth-page">
      <div className="auth-bg" style={{ background: 'linear-gradient(180deg,#dce8f5 0%,#b8d4ed 30%,#8fbfde 60%,#6aaa9e 80%,#4a8a6a 100%)' }} />
      <div className="auth-hills" style={{ background: 'linear-gradient(180deg,#6ab04c 0%,#218c74 100%)', height: '50%' }} />
      <div className="auth-hills-2" style={{ background: 'linear-gradient(180deg,#55a340 0%,#1a6b3a 100%)', height: '38%' }} />
      <div className="auth-card" style={{ margin: '80px 16px 16px' }}>
        {children}
      </div>
    </div>
  );
}

export function BuyerRegister() {
  const { register } = useAuth();
  const navigate = useNavigate();
  const [form, setForm] = useState({ email: '', password: '', full_name: '', phone: '', zip_code: '' });
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(false);

  const set = (k) => (e) => setForm(f => ({ ...f, [k]: e.target.value }));

  const handleSubmit = async (e) => {
    e.preventDefault();
    setLoading(true);
    setError('');

    try {
      await register({ ...form, role: 'buyer' });
      navigate('/home');
    } catch (err) {
      setError(err.message);
    } finally {
      setLoading(false);
    }
  };

  return (
    <AuthLayout>
      <h2 className="auth-title">Create Buyer Account</h2>
      <p className="auth-subtitle">Join local farmers and fresh produce</p>
      {error && <div className="alert alert-error">{error}</div>}

      <form className="auth-form" onSubmit={handleSubmit}>
        <div className="form-group">
          <label className="form-label">Full Name</label>
          <input className="form-input" placeholder="Your name" value={form.full_name} onChange={set('full_name')} />
        </div>

        <div className="form-group">
          <label className="form-label">Email</label>
          <input className="form-input" type="email" placeholder="you@example.com" required value={form.email} onChange={set('email')} />
        </div>

        <div className="form-group">
          <label className="form-label">Password</label>
          <input className="form-input" type="password" placeholder="Create a password" required value={form.password} onChange={set('password')} />
        </div>

        <div className="grid-2">
          <div className="form-group">
            <label className="form-label">Phone</label>
            <input className="form-input" placeholder="555-1234" value={form.phone} onChange={set('phone')} />
          </div>

          <div className="form-group">
            <label className="form-label">Zip Code</label>
            <input className="form-input" placeholder="93701" value={form.zip_code} onChange={set('zip_code')} />
          </div>
        </div>

        <button className="btn btn-primary btn-lg btn-full" type="submit" disabled={loading}>
          {loading ? 'Creating account...' : 'Register as Buyer'}
        </button>
      </form>

      <div className="auth-links">
        Already have an account? <Link to="/buyer/login">Sign in</Link>
        <br />
        <Link to="/seller/register">Register as a Seller instead</Link>
      </div>
    </AuthLayout>
  );
}

export function SellerRegister() {
  const { register } = useAuth();
  const navigate = useNavigate();
  const [form, setForm] = useState({
    email: '',
    password: '',
    farm_name: '',
    biography: '',
    pickup_address: '',
    operating_hours: '',
    zip_code: '',
  });
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(false);

  const set = (k) => (e) => setForm(f => ({ ...f, [k]: e.target.value }));

  const handleSubmit = async (e) => {
    e.preventDefault();
    setLoading(true);
    setError('');

    try {
      await register({ ...form, role: 'seller' });
      navigate('/home');
    } catch (err) {
      setError(err.message);
    } finally {
      setLoading(false);
    }
  };

  return (
    <AuthLayout>
      <h2 className="auth-title">Register Your Farm</h2>
      <p className="auth-subtitle">Reach local buyers with your fresh produce</p>
      {error && <div className="alert alert-error">{error}</div>}

      <form className="auth-form" onSubmit={handleSubmit}>
        <div className="form-group">
          <label className="form-label">Email</label>
          <input className="form-input" type="email" placeholder="farm@example.com" required value={form.email} onChange={set('email')} />
        </div>

        <div className="form-group">
          <label className="form-label">Password</label>
          <input className="form-input" type="password" placeholder="Create a password" required value={form.password} onChange={set('password')} />
        </div>

        <div className="form-group">
          <label className="form-label">Farm Name</label>
          <input className="form-input" placeholder="Cheshire Farms" value={form.farm_name} onChange={set('farm_name')} />
        </div>

        <div className="form-group">
          <label className="form-label">Biography</label>
          <textarea className="form-textarea" rows={2} placeholder="Tell buyers about your farm..." value={form.biography} onChange={set('biography')} />
        </div>

        <div className="form-group">
          <label className="form-label">Pickup Address</label>
          <input className="form-input" placeholder="123 Farm Rd, Fresno CA" value={form.pickup_address} onChange={set('pickup_address')} />
        </div>

        <div className="grid-2">
          <div className="form-group">
            <label className="form-label">Operating Hours</label>
            <input className="form-input" placeholder="Mon-Sat 8am-5pm" value={form.operating_hours} onChange={set('operating_hours')} />
          </div>

          <div className="form-group">
            <label className="form-label">Zip Code</label>
            <input className="form-input" placeholder="93701" value={form.zip_code} onChange={set('zip_code')} />
          </div>
        </div>

        <button className="btn btn-green btn-lg btn-full" type="submit" disabled={loading}>
          {loading ? 'Creating farm...' : 'Register as Seller'}
        </button>
      </form>

      <div className="auth-links">
        Already have an account? <Link to="/seller/login">Sign in</Link>
        <br />
        <Link to="/buyer/register">Register as a Buyer instead</Link>
      </div>
    </AuthLayout>
  );
}

export function BuyerLogin() {
  const { login } = useAuth();
  const navigate = useNavigate();
  const [form, setForm] = useState({ email: '', password: '' });
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(false);

  const set = (k) => (e) => setForm(f => ({ ...f, [k]: e.target.value }));

  const handleSubmit = async (e) => {
    e.preventDefault();
    setLoading(true);
    setError('');

    try {
      const user = await login(form);
      if (user.role !== 'buyer') {
        setError('This account is not a buyer.');
        return;
      }
      navigate('/home');
    } catch (err) {
      setError(err.message);
    } finally {
      setLoading(false);
    }
  };

  return (
    <AuthLayout>
      <h2 className="auth-title">Welcome Back</h2>
      <p className="auth-subtitle">Sign in to your buyer account</p>
      {error && <div className="alert alert-error">{error}</div>}

      <form className="auth-form" onSubmit={handleSubmit}>
        <div className="form-group">
          <label className="form-label">Email</label>
          <input className="form-input" type="email" placeholder="Enter your email" required value={form.email} onChange={set('email')} />
        </div>

        <div className="form-group">
          <label className="form-label">Password</label>
          <input className="form-input" type="password" placeholder="Enter your password" required value={form.password} onChange={set('password')} />
        </div>

        <button className="btn btn-primary btn-lg btn-full" type="submit" disabled={loading}>
          {loading ? 'Signing in...' : 'Sign In'}
        </button>

        <div style={{ margin: '12px 0', textAlign: 'center' }}>OR</div>

        <OAuthButton role="buyer" />
      </form>

      <div className="auth-links">
        No account? <Link to="/buyer/register">Register as Buyer</Link>
        <br />
        <Link to="/seller/login">Sign in as Seller instead</Link>
        <br /><br />
        <Link to="/" className="btn btn-danger btn-sm">← Back</Link>
      </div>
    </AuthLayout>
  );
}

export function SellerLogin() {
  const { login } = useAuth();
  const navigate = useNavigate();
  const [form, setForm] = useState({ email: '', password: '' });
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(false);

  const set = (k) => (e) => setForm(f => ({ ...f, [k]: e.target.value }));

  const handleSubmit = async (e) => {
    e.preventDefault();
    setLoading(true);
    setError('');

    try {
      const user = await login(form);
      if (user.role !== 'seller') {
        setError('This account is not a seller.');
        return;
      }
      navigate('/home');
    } catch (err) {
      setError(err.message);
    } finally {
      setLoading(false);
    }
  };

  return (
    <AuthLayout>
      <h2 className="auth-title">Seller Sign In</h2>
      <p className="auth-subtitle">Access your farm dashboard</p>
      {error && <div className="alert alert-error">{error}</div>}

      <form className="auth-form" onSubmit={handleSubmit}>
        <div className="form-group">
          <label className="form-label">Email</label>
          <input className="form-input" type="email" placeholder="Enter your email" required value={form.email} onChange={set('email')} />
        </div>

        <div className="form-group">
          <label className="form-label">Password</label>
          <input className="form-input" type="password" placeholder="Enter your password" required value={form.password} onChange={set('password')} />
        </div>

        <button className="btn btn-green btn-lg btn-full" type="submit" disabled={loading}>
          {loading ? 'Signing in...' : 'Sign In as Seller'}
        </button>

        <div style={{ margin: '12px 0', textAlign: 'center' }}>OR</div>

        <OAuthButton role="seller" />
      </form>

      <div className="auth-links">
        No account? <Link to="/seller/register">Register Your Farm</Link>
        <br />
        <Link to="/buyer/login">Sign in as Buyer instead</Link>
        <br /><br />
        <Link to="/" className="btn btn-danger btn-sm">← Back</Link>
      </div>
    </AuthLayout>
  );
}