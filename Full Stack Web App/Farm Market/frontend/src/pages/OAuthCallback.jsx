import React, { useEffect, useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';

export default function OAuthCallback() {
  const navigate = useNavigate();
  const { completeOAuthLogin } = useAuth();
  const [message, setMessage] = useState('Completing OAuth login...');

  useEffect(() => {
    const params = new URLSearchParams(window.location.search);
    const token = params.get('token');
    const error = params.get('error');

    if (error) {
      setMessage(error);
      return;
    }

    if (!token) {
      setMessage('OAuth login did not return a token.');
      return;
    }

    completeOAuthLogin(token)
      .then(() => navigate('/home'))
      .catch((err) => setMessage(err.message));
  }, [completeOAuthLogin, navigate]);

  return (
    <div className="auth-page">
      <div className="auth-card" style={{ margin: '120px auto' }}>
        <h2 className="auth-title">OAuth Login</h2>
        <p className="auth-subtitle">{message}</p>
      </div>
    </div>
  );
}