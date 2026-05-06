import { useState } from 'react';
import Navbar from './components/Navbar.jsx';
import { Routes, Route, Navigate } from 'react-router-dom';
import Landing from './pages/Landing.jsx';
import Dashboard from './pages/Dashboard.jsx';
import LogActivity from './pages/LogActivity.jsx';
import LogHistory from './pages/LogHistory.jsx';
import Settings from './pages/Settings.jsx';
import SignIn from './pages/SignIn.jsx';
import SignUp from './pages/SignUp.jsx';
import ForgotPassword from './pages/ForgotPassword.jsx';
import Survey from './pages/Survey.jsx';
import './App.css';

function App() {
  const [isAuthenticated, setIsAuthenticated] = useState(false);

  return (
    <>
      <Navbar />
      <div className="content">
        <Routes>
          {/* Public Routes */}
          <Route path="/" element={<Landing />} />
          <Route path="/signin" element={<SignIn onLogin={() => setIsAuthenticated(true)} />} />
          <Route path="/signup" element={<SignUp onRegister={() => setIsAuthenticated(true)} />} />
          <Route path="/forgot-password" element={<ForgotPassword />} />
          
          {/* Protected Routes */}
          <Route 
            path="/dashboard" 
            element={isAuthenticated ? <Dashboard /> : <Navigate to="/signin" />} 
          />
          <Route 
            path="/log-activity" 
            element={isAuthenticated ? <LogActivity /> : <Navigate to="/signin" />} 
          />
          <Route 
            path="/log-history" 
            element={isAuthenticated ? <LogHistory /> : <Navigate to="/signin" />} 
          />
          <Route 
            path="/settings" 
            element={isAuthenticated ? <Settings /> : <Navigate to="/signin" />} 
          />
          <Route 
            path="/survey" 
            element={isAuthenticated ? <Survey /> : <Navigate to="/signin" />} 
          />
        </Routes>
      </div>
    </>
  );
}

export default App;