import React, { useState, useEffect } from 'react';
import { membershipApi } from '../api';
import { AppShell } from '../components';
import { useAuth } from '../context/AuthContext';

function PlanCard({ plan, price, features, current, onUpgrade, featured }) {
  const isFree = price === 0;
  const isCurrent = current === plan;

  return (
    <div className={`plan-card${featured ? ' featured' : ''}`} style={{
      opacity: isCurrent ? 1 : 0.9,
      transition: 'transform 0.2s',
      transform: featured ? 'scale(1.02)' : 'scale(1)',
    }}>
      {featured && (
        <div style={{
          background: 'var(--purple)', color: '#fff',
          fontSize: 11, fontWeight: 700, letterSpacing: 1,
          textTransform: 'uppercase', padding: '4px 12px',
          borderRadius: 'var(--radius-full)', alignSelf: 'center',
          marginBottom: 4,
        }}>
          ⭐ RECOMMENDED
        </div>
      )}
      <h3 style={{ fontFamily: 'var(--font-display)', fontWeight: 700, fontSize: 22, color: '#040000' }}>
        {plan.replace(/_/g, ' ').replace(/\b\w/g, c => c.toUpperCase())}
      </h3>
      <ul style={{ listStyle: 'disc', paddingLeft: 20, textAlign: 'left', margin: '8px 0', display: 'flex', flexDirection: 'column', gap: 6 }}>
        {features.map(f => (
          <li key={f} style={{ fontSize: 13, color: 'var(--gray-800)' }}>{f}</li>
        ))}
      </ul>
      <div
        className={`plan-price${isFree ? ' free-plan' : ''}`}
        onClick={() => !isCurrent && onUpgrade(plan)}
        style={{ cursor: isCurrent ? 'default' : 'pointer', opacity: isCurrent ? 0.7 : 1 }}
      >
        {isCurrent ? '✓ Current Plan' : isFree ? 'Free' : `$${price.toFixed(2)}/month`}
      </div>
    </div>
  );
}

export default function Membership() {
  const { user } = useAuth();
  const [plans, setPlans] = useState(null);
  const [membership, setMembership] = useState(null);
  const [loading, setLoading] = useState(true);
  const [msg, setMsg] = useState('');
  const [error, setError] = useState('');

  useEffect(() => {
    Promise.all([membershipApi.plans(), membershipApi.get()])
      .then(([p, m]) => { setPlans(p); setMembership(m); })
      .catch(console.error)
      .finally(() => setLoading(false));
  }, []);

  const upgrade = async (plan) => {
    try {
      const data = await membershipApi.upgrade(plan);
      setMembership(data.membership);
      setMsg(`🎉 Upgraded to ${plan.replace(/_/g, ' ')}! ${data.price > 0 ? `$${data.price.toFixed(2)}/month` : 'Free'}`);
      setTimeout(() => setMsg(''), 4000);
    } catch (e) {
      setError(e.message);
      setTimeout(() => setError(''), 4000);
    }
  };

  const isBuyer = user?.role === 'buyer';
  const availablePlans = isBuyer ? plans?.buyer_plans : plans?.seller_plans;

  return (
    <AppShell>
      {/* Hero */}
      <div style={{
        background: 'linear-gradient(135deg,#e8f4fd 0%,#d6eaf8 50%,#a9cce3 100%)',
        borderRadius: 'var(--radius-lg)',
        padding: '40px',
        marginBottom: 32,
        position: 'relative',
        overflow: 'hidden',
        textAlign: 'center',
      }}>
        {/* Decorative elements */}
        <div style={{
          position: 'absolute', bottom: -10, left: '-5%', right: '-5%',
          height: 50, background: 'linear-gradient(180deg,#a8d5b5 0%,#6ab04c 100%)',
          borderRadius: '50% 50% 0 0 / 30% 30% 0 0',
        }} />
        <div style={{ position: 'relative', zIndex: 1 }}>
          <h1 style={{ fontFamily: 'var(--font-display)', fontWeight: 800, fontSize: 32, marginBottom: 8 }}>
            🎖 Membership Plans
          </h1>
          <p style={{ color: 'var(--gray-600)', fontSize: 15 }}>
            {isBuyer ? 'Unlock premium buyer features' : 'Grow your farm business'}
          </p>
          {membership && (
            <div style={{ marginTop: 16, display: 'inline-block', background: '#fff', borderRadius: 'var(--radius-full)', padding: '6px 20px', fontWeight: 600, fontSize: 14 }}>
              Current plan: <span style={{ color: 'var(--purple)', fontFamily: 'var(--font-display)' }}>
                {membership.plan.replace(/_/g, ' ').replace(/\b\w/g, c => c.toUpperCase())}
              </span>
              {membership.expires_at && (
                <span style={{ color: 'var(--gray-600)', fontWeight: 400, marginLeft: 8, fontSize: 12 }}>
                  · Renews {new Date(membership.expires_at).toLocaleDateString()}
                </span>
              )}
            </div>
          )}
        </div>
      </div>

      {msg   && <div className="alert alert-success">{msg}</div>}
      {error && <div className="alert alert-error">{error}</div>}

      {loading ? (
        <div className="spinner" />
      ) : (
        <div style={{
          display: 'grid',
          gridTemplateColumns: `repeat(${availablePlans?.length || 2}, 1fr)`,
          gap: 24,
          maxWidth: 700,
          margin: '0 auto',
        }}>
          {availablePlans?.map((p, i) => (
            <PlanCard
              key={p.plan}
              plan={p.plan}
              price={p.price}
              features={p.features}
              current={membership?.plan}
              onUpgrade={upgrade}
              featured={i === availablePlans.length - 1 && availablePlans.length > 1}
            />
          ))}
        </div>
      )}
    </AppShell>
  );
}
