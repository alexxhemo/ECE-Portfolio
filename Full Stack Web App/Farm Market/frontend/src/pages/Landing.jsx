import React from 'react';
import { Link } from 'react-router-dom';
import { MacDonald } from '../components';
import '../styles/global.css';

export default function Landing() {
  return (
    <div className="auth-page" style={{ minHeight: '100vh' }}>
      {/* Sky background */}
      <div className="auth-bg" style={{ background: 'linear-gradient(180deg,#dce8f5 0%,#b8d4ed 30%,#8fbfde 60%,#6aaa9e 80%,#4a8a6a 100%)' }} />
      {/* Hills */}
      <div className="auth-hills" style={{ background: 'linear-gradient(180deg,#6ab04c 0%,#218c74 100%)', height: '50%' }} />
      <div className="auth-hills-2" style={{ background: 'linear-gradient(180deg,#55a340 0%,#1a6b3a 100%)', height: '38%' }} />

      {/* Sparkles */}
      {['12%,28%','22%,42%','45%,35%','68%,22%','78%,38%','88%,30%'].map((pos, i) => (
        <div key={i} style={{
          position: 'absolute',
          left: pos.split(',')[0], top: pos.split(',')[1],
          color: '#7bed9f', fontSize: 18, zIndex: 3, animation: `spin ${2 + i * 0.3}s linear infinite`,
        }}>✦</div>
      ))}

      {/* Top nav buttons */}
      <div style={{
        position: 'absolute', top: 24, left: 0, right: 0,
        display: 'flex', justifyContent: 'space-between', padding: '0 32px', zIndex: 10,
      }}>
        <div style={{ display: 'flex', gap: 16 }}>
          <Link to="/buyer/register" className="btn btn-outline" style={{ background: '#fff', fontFamily: 'var(--font-display)', fontWeight: 600 }}>
            Register as Buyer
          </Link>
          <Link to="/buyer/login" className="btn btn-outline" style={{ background: '#fff', fontFamily: 'var(--font-display)', fontWeight: 600 }}>
            Sign-in as Buyer
          </Link>
        </div>
        <div style={{ display: 'flex', gap: 16 }}>
          <Link to="/seller/register" className="btn btn-outline" style={{ background: '#fff', fontFamily: 'var(--font-display)', fontWeight: 600 }}>
            Become a Seller
          </Link>
          <Link to="/seller/login" className="btn btn-outline" style={{ background: '#fff', fontFamily: 'var(--font-display)', fontWeight: 600 }}>
            Sign-in as Seller
          </Link>
        </div>
      </div>

      {/* Hero title */}
      <div style={{ position: 'relative', zIndex: 10, textAlign: 'center' }}>
        <h1 style={{
          fontFamily: 'var(--font-display)',
          fontWeight: 800,
          fontSize: 'clamp(40px, 7vw, 80px)',
          color: '#1a1a1a',
          textShadow: '0 2px 20px rgba(255,255,255,0.6)',
          letterSpacing: '-1px',
          lineHeight: 1.15,
        }}>
          Farmer's Market<br />Online
        </h1>
        <p style={{
          marginTop: 16, fontSize: 18, color: '#2d2d2d',
          fontFamily: 'var(--font-body)', fontWeight: 400,
          textShadow: '0 1px 6px rgba(255,255,255,0.5)',
        }}>
          Fresh from local farms, delivered to your community
        </p>
        <div style={{ marginTop: 32, display: 'flex', gap: 16, justifyContent: 'center' }}>
          <Link to="/buyer/register" className="btn btn-green-light" style={{ fontSize: 16 }}>
            Shop Now
          </Link>
          <Link to="/seller/register" className="btn" style={{
            background: 'rgba(255,255,255,0.85)', color: 'var(--green-dark)',
            borderRadius: 'var(--radius-full)', border: '3px solid var(--amber)',
            fontFamily: 'var(--font-display)', fontWeight: 700, fontSize: 16, padding: '12px 32px',
          }}>
            Sell Your Produce
          </Link>
        </div>
      </div>

      <MacDonald />
    </div>
  );
}
