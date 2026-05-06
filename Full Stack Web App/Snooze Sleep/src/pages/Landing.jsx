import { Link } from 'react-router-dom';

export default function Landing() {
  return (
    <div className="page-container">
      <h1>Welcome to SnoozeStreak</h1>
      <p>Your personal sleep tracking companion</p>
      <Link to="/signin" className="journey-button">
        Click here to begin your personal journey
      </Link>
    </div>
  );
}