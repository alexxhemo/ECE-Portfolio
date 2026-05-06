import { useState } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import './Auth.css'; // Reusing your auth styles

export default function ForgotPassword() {
  const [email, setEmail] = useState('');
  const [message, setMessage] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  const navigate = useNavigate();

  const handleSubmit = async (e) => {
    e.preventDefault();
    setIsLoading(true);
    
    try {
      // Simulate API call
      await new Promise(resolve => setTimeout(resolve, 1000));
      setMessage('Password reset link sent to your email');
    } catch (error) {
      setMessage('Failed to send reset link');
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <div className="auth-form">
      <h2>Reset Password</h2>
      {message ? (
        <>
          <p className="success-message">{message}</p>
          <button 
            className="auth-button"
            onClick={() => navigate('/signin')}
          >
            Back to Sign In
          </button>
        </>
      ) : (
        <>
          <p>Enter your email to receive a reset link</p>
          <form onSubmit={handleSubmit}>
            <input
              type="email"
              placeholder="Email"
              value={email}
              onChange={(e) => setEmail(e.target.value)}
              required
            />
            <div className="button-group">
              <button 
                type="submit" 
                className="auth-button"
                disabled={isLoading}
              >
                {isLoading ? 'Sending...' : 'Reset Password'}
              </button>
              <button 
                type="button" 
                className="auth-button secondary"
                onClick={() => navigate('/signin')}
              >
                Cancel
              </button>
            </div>
          </form>
        </>
      )}
      <div className="auth-links">
        <Link to="/signup">Don't have an account? Sign Up</Link>
      </div>
    </div>
  );
}