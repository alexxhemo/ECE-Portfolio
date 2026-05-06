import { useState, useEffect } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import PasswordStrength from '../components/PasswordStrength';
import './Auth.css';

export default function SignUp({ onRegister }) {
  const [formData, setFormData] = useState({
    firstName: '',
    lastName: '',
    email: '',
    password: ''
  });
  const [passwordValid, setPasswordValid] = useState(false);
  const navigate = useNavigate();

  const checkPasswordStrength = (password) => {
    const hasMinLength = password.length >= 8;
    const hasUpperCase = /[A-Z]/.test(password);
    const hasLowerCase = /[a-z]/.test(password);
    const hasNumber = /[0-9]/.test(password);
    return hasMinLength && hasUpperCase && hasLowerCase && hasNumber;
  };

  useEffect(() => {
    setPasswordValid(checkPasswordStrength(formData.password));
  }, [formData.password]);

  const handleChange = (e) => {
    setFormData({...formData, [e.target.name]: e.target.value});
  };

  const handleSubmit = (e) => {
    e.preventDefault();
    if (passwordValid) {
      onRegister();
      navigate('/survey');
    }
  };

  return (
    <div className="auth-form">
      <h2>Register</h2>
      <form onSubmit={handleSubmit}>
        <input
          name="firstName"
          placeholder="First Name"
          value={formData.firstName}
          onChange={handleChange}
          required
        />
        <input
          name="lastName"
          placeholder="Last Name"
          value={formData.lastName}
          onChange={handleChange}
          required
        />
        <input
          name="email"
          type="email"
          placeholder="Email"
          value={formData.email}
          onChange={handleChange}
          required
        />
        <div className="password-input">
          <input
            name="password"
            type="password"
            placeholder="Password"
            value={formData.password}
            onChange={handleChange}
            required
          />
          <PasswordStrength password={formData.password} />
        </div>
        <button 
          type="submit" 
          className="auth-button"
          disabled={!passwordValid}
        >
          Register
        </button>
      </form>
      <div className="auth-links">
        <Link to="/signin">Already have an account? Sign In</Link>
      </div>
    </div>
  );
}