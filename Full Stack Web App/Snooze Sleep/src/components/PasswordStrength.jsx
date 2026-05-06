import { useEffect, useState } from 'react';
import '../PasswordStrength.css';

export default function PasswordStrength({ password }) {
  const [strength, setStrength] = useState(0);
  const [requirements, setRequirements] = useState({
    length: false,
    upperCase: false,
    lowerCase: false,
    number: false
  });

  useEffect(() => {
    const newRequirements = {
      length: password.length >= 8,
      upperCase: /[A-Z]/.test(password),
      lowerCase: /[a-z]/.test(password),
      number: /[0-9]/.test(password)
    };
    setRequirements(newRequirements);
    
    // Calculate strength (0-4)
    const newStrength = Object.values(newRequirements).filter(Boolean).length;
    setStrength(newStrength);
  }, [password]);

  const getStrengthColor = () => {
    if (strength === 0) return '#ff0000';
    if (strength === 1) return '#ff4d4d';
    if (strength === 2) return '#ffa500';
    if (strength === 3) return '#99cc33';
    return '#33cc33';
  };

  return (
    <div className="password-strength">
      <div className="strength-meter">
        <div 
          className="strength-bar" 
          style={{
            width: `${strength * 25}%`,
            backgroundColor: getStrengthColor()
          }}
        ></div>
      </div>
      <div className="requirements">
        <p className={requirements.length ? 'valid' : ''}>
          {requirements.length ? '✓' : '•'} At least 8 characters
        </p>
        <p className={requirements.upperCase ? 'valid' : ''}>
          {requirements.upperCase ? '✓' : '•'} 1 uppercase letter
        </p>
        <p className={requirements.lowerCase ? 'valid' : ''}>
          {requirements.lowerCase ? '✓' : '•'} 1 lowercase letter
        </p>
        <p className={requirements.number ? 'valid' : ''}>
          {requirements.number ? '✓' : '•'} 1 number
        </p>
      </div>
    </div>
  );
}