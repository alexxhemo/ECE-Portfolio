import React, { useState, useEffect } from 'react';
import { profileApi } from '../api';
import { AppShell } from '../components';
import { useAuth } from '../context/AuthContext';

function ProfileHeader({ title, color = '#148ebc' }) {
  return (
    <div style={{
      background: `linear-gradient(135deg, ${color}66, ${color}22)`,
      borderRadius: 'var(--radius-md)',
      padding: '16px 24px',
      marginBottom: 24,
      borderLeft: `4px solid ${color}`,
    }}>
      <h1 style={{ fontFamily: 'var(--font-display)', fontWeight: 800, fontSize: 36, color: 'var(--gray-800)' }}>
        {title}
      </h1>
    </div>
  );
}

function FieldRow({ label, value, editing, inputKey, form, setForm, type = 'text', multiline = false }) {
  return (
    <div className="form-group">
      <label className="form-label" style={{ opacity: 0.8 }}>{label}</label>
      {editing ? (
        multiline ? (
          <textarea
            className="form-textarea"
            value={form[inputKey] || ''}
            onChange={e => setForm(f => ({ ...f, [inputKey]: e.target.value }))}
            rows={3}
          />
        ) : (
          <input
            className="form-input"
            type={type}
            value={form[inputKey] || ''}
            onChange={e => setForm(f => ({ ...f, [inputKey]: e.target.value }))}
          />
        )
      ) : (
        <div style={{
          padding: '12px 16px',
          background: 'var(--gray-50)',
          borderRadius: 'var(--radius-sm)',
          border: '1px solid var(--gray-200)',
          fontSize: 15,
          color: value ? 'var(--gray-800)' : 'var(--gray-400)',
          minHeight: 46,
        }}>
          {value || <em style={{ opacity: 0.5 }}>Not set</em>}
        </div>
      )}
    </div>
  );
}

export function BuyerProfile() {
  const { user } = useAuth();
  const [profile, setProfile] = useState(null);
  const [editing, setEditing] = useState(false);
  const [form, setForm] = useState({ full_name: '', phone: '', zip_code: '' });
  const [saving, setSaving] = useState(false);
  const [msg, setMsg] = useState('');
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const loadProfile = async () => {
      setLoading(true);
      try {
        setError('');
        const d = await profileApi.getBuyer();
        setProfile(d);
        setForm(d || { full_name: '', phone: '', zip_code: '' });
      } catch (e) {
        if (e.message && e.message.toLowerCase().includes('not found')) {
          const empty = { full_name: '', phone: '', zip_code: '' };
          setProfile(empty);
          setForm(empty);
        } else {
          setError(e.message);
        }
      } finally {
        setLoading(false);
      }
    };

    loadProfile();
  }, []);

  const save = async () => {
    setSaving(true);
    setMsg('');
    setError('');
    try {
      const updated = await profileApi.updateBuyer(form);
      setProfile(updated);
      setForm(updated);
      setEditing(false);
      setMsg('Profile saved! ✓');
      setTimeout(() => setMsg(''), 3000);
    } catch (e) {
      setError(e.message);
    } finally {
      setSaving(false);
    }
  };

  return (
    <AppShell>
      {/* Farm scene banner */}
      <div style={{
        background: 'linear-gradient(90deg,#56a3a6 0%,#1a8a9a 50%,#1a6a7a 100%)',
        height: 80, borderRadius: 'var(--radius-md)', marginBottom: 24,
        position: 'relative', overflow: 'hidden',
      }}>
        <div style={{ position: 'absolute', bottom: 0, left: 0, right: 0, height: 30,
          background: 'linear-gradient(180deg,#6ab04c 0%,#2d8e60 100%)',
          borderRadius: '50% 50% 0 0 / 30% 30% 0 0',
        }}/>
      </div>

      <ProfileHeader title="Buyer Profile" color="#148ebc" />

      {loading && <div className="spinner" />}
      {error && <div className="alert alert-error">{error}</div>}
      {msg && <div className="alert alert-success">{msg}</div>}

      {/* Edit button row */}
      <div style={{ display: 'flex', justifyContent: 'flex-end', marginBottom: 24 }}>
        {editing ? (
          <div style={{ display: 'flex', gap: 12 }}>
            <button
              className="btn btn-outline"
              onClick={() => {
                setEditing(false);
                setError('');
                setForm(profile || {
                  farm_name: '',
                  pickup_address: '',
                  biography: '',
                  operating_hours: '',
                  zip_code: '',
                });
              }}
            >
              Cancel
            </button>
            <button className="btn btn-green" onClick={save} disabled={saving}>
              {saving ? 'Saving...' : 'Save Changes'}
            </button>
          </div>
        ) : (
          <button className="btn btn-primary" onClick={() => setEditing(true)} style={{ background: '#4182f9' }}>
            Edit
          </button>
        )}
      </div>

      <div className="grid-2" style={{ gap: 24 }}>
        <FieldRow label="Name"          value={profile?.full_name} inputKey="full_name" editing={editing} form={form} setForm={setForm} />
        <FieldRow label="Phone"         value={profile?.phone}     inputKey="phone"     editing={editing} form={form} setForm={setForm} />
        <FieldRow label="Email"         value={user?.email}        inputKey="_email"    editing={false}   form={form} setForm={setForm} />
        <FieldRow label="Home Location (Zip)" value={profile?.zip_code} inputKey="zip_code" editing={editing} form={form} setForm={setForm} />
      </div>
    </AppShell>
  );
}

export function SellerProfile() {
  const { user } = useAuth();
  const [profile, setProfile] = useState(null);
  const [editing, setEditing] = useState(false);
  const [form, setForm] = useState({
    farm_name: '',
    pickup_address: '',
    biography: '',
    operating_hours: '',
    zip_code: '',
  });
  const [saving, setSaving] = useState(false);
  const [msg, setMsg] = useState('');
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(true);
  
  useEffect(() => {
    const loadProfile = async () => {
      setLoading(true);
      try {
        setError('');
        const d = await profileApi.getSeller();
        setProfile(d);
        setForm(d || {
          farm_name: '',
          pickup_address: '',
          biography: '',
          operating_hours: '',
          zip_code: '',
        });
      } catch (e) {
        if (e.message && e.message.toLowerCase().includes('not found')) {
          const empty = {
            farm_name: '',
            pickup_address: '',
            biography: '',
            operating_hours: '',
            zip_code: '',
          };
          setProfile(empty);
          setForm(empty);
        } else {
          setError(e.message);
        }
      } finally {
        setLoading(false);
      }
    };

    loadProfile();
  }, []);
  
  const save = async () => {
    setSaving(true);
    setMsg('');
    setError('');
    try {
      const updated = await profileApi.updateSeller(form);
      setProfile(updated);
      setForm(updated);
      setEditing(false);
      setMsg('Profile saved! ✓');
      setTimeout(() => setMsg(''), 3000);
    } catch (e) {
      setError(e.message);
    } finally {
      setSaving(false);
    }
  };

  return (
    <AppShell>
      {/* Farm scene banner */}
      <div style={{
        background: 'linear-gradient(90deg,#4a8a6a 0%,#2d7a5a 50%,#1a6a4a 100%)',
        height: 80, borderRadius: 'var(--radius-md)', marginBottom: 24,
        position: 'relative', overflow: 'hidden',
      }}>
        <div style={{ position: 'absolute', bottom: 0, left: 0, right: 0, height: 30,
          background: 'linear-gradient(180deg,#6ab04c 0%,#2d8e60 100%)',
          borderRadius: '50% 50% 0 0 / 30% 30% 0 0',
        }}/>
      </div>

      <ProfileHeader title="Seller Profile" color="#396d56" />

      {loading && <div className="spinner" />}
      {error && <div className="alert alert-error">{error}</div>}
      {msg && <div className="alert alert-success">{msg}</div>}

      <div style={{ display: 'flex', justifyContent: 'flex-end', marginBottom: 24 }}>
        {editing ? (
          <div style={{ display: 'flex', gap: 12 }}>
            <button
              className="btn btn-outline"
              onClick={() => {
                setEditing(false);
                setError('');
                setForm(profile || { full_name: '', phone: '', zip_code: '' });
              }}
            >
              Cancel
            </button>
            <button className="btn btn-green" onClick={save} disabled={saving}>
              {saving ? 'Saving...' : 'Save Changes'}
            </button>
          </div>
        ) : (
          <button className="btn btn-primary" onClick={() => setEditing(true)} style={{ background: '#4182f9' }}>
            Edit
          </button>
        )}
      </div>

      <div className="grid-2" style={{ gap: 24 }}>
        <FieldRow label="Farm Name"       value={profile?.farm_name}       inputKey="farm_name"       editing={editing} form={form} setForm={setForm} />
        <FieldRow label="Pickup Address"  value={profile?.pickup_address}  inputKey="pickup_address"  editing={editing} form={form} setForm={setForm} />
        <FieldRow label="Biography"       value={profile?.biography}       inputKey="biography"       editing={editing} form={form} setForm={setForm} multiline />
        <FieldRow label="Operating Hours" value={profile?.operating_hours} inputKey="operating_hours" editing={editing} form={form} setForm={setForm} />
        <FieldRow label="Email"           value={user?.email}              inputKey="_email"          editing={false}   form={form} setForm={setForm} />
        <FieldRow label="Zip Code"        value={profile?.zip_code}        inputKey="zip_code"        editing={editing} form={form} setForm={setForm} />
      </div>
    </AppShell>
  );
}

export default function Profile() {
  const { user } = useAuth();
  if (user?.role === 'seller') return <SellerProfile />;
  return <BuyerProfile />;
}
