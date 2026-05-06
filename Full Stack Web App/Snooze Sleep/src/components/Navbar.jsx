import SnoozeStreak from '../assets/logo.png'
import './Navbar.css';

const Navbar = () => {
  return (

<nav className="navbar">
    <div className="navbar-left">
        <a href="/" className="logo">
            <img src={SnoozeStreak} alt="SnoozeStreak Logo" className="logo-img"/>
        </a>
    </div>
    <div className="navbar-center">
        <ul className="nav-links">
            <li>
                <a href="/">Landing</a>
            </li>
            <li>
                <a href="/dashboard">Dashboard</a>
            </li>
            <li>
                <a href="/log-activity">Log Activity</a>
            </li>
            <li>
                <a href="/log-history">Log History</a>
            </li>
        </ul>
    </div>
    <div className="navbar-right">
        <a href="/menu" className="menu-icon">
            <i className="fas fa-bars"></i>
        </a>
        <a href="/settings" className="gear-icon">
            <i className="fas fa-cog"></i>
        </a>
    </div>
</nav>
);
};

export default Navbar;