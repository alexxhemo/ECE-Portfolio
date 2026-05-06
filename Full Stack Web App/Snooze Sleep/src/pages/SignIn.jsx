import { useState } from 'react';
import { Link, useNavigate } from 'react-router-dom';

export default function SignIn({ onLogin }) {
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const navigate = useNavigate();

  const handleSubmit = (e) => {
    e.preventDefault();
    // Authentication logic here
    onLogin();
    navigate('/dashboard'); // Changed from /survey to /dashboard
  };

  return (
    <div className="auth-form">
      <h2>Sign In</h2>
      <form onSubmit={handleSubmit}>
        <input
          type="email"
          placeholder="Email"
          value={email}
          onChange={(e) => setEmail(e.target.value)}
          required
        />
        <input
          type="password"
          placeholder="Password"
          value={password}
          onChange={(e) => setPassword(e.target.value)}
          required
        />
        <button type="submit">Sign In</button>
      </form>
      <div className="auth-links">
        <Link to="/forgot-password">Forgot password?</Link>
        <Link to="/signup">Don't have an account? Register</Link>
      </div>
    </div>
  );
}