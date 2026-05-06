import React from 'react';
import { BrowserRouter, Routes, Route, Navigate } from 'react-router-dom';
import { AuthProvider } from './context/AuthContext';
import { ProtectedRoute } from './components';
import './styles/global.css';

// Pages
import Landing      from './pages/Landing';
import { BuyerRegister, SellerRegister, BuyerLogin, SellerLogin } from './pages/Auth';
import Home         from './pages/Home';
import Marketplace  from './pages/Marketplace';
import Cart         from './pages/Cart';
import Profile      from './pages/Profile';
import MyOrders     from './pages/MyOrders';
import IncomingOrders from './pages/IncomingOrders';
import MyListings, { CreateListing } from './pages/MyListings';
import Reviews      from './pages/Reviews';
import Membership   from './pages/Membership';
import OAuthCallback from './pages/OAuthCallback';

export default function App() {
  return (
    <AuthProvider>
      <BrowserRouter>
        <Routes>
          {/* Public */}
          <Route path="/"                element={<Landing />} />
          <Route path="/buyer/register"  element={<BuyerRegister />} />
          <Route path="/buyer/login"     element={<BuyerLogin />} />
          <Route path="/seller/register" element={<SellerRegister />} />
          <Route path="/seller/login"    element={<SellerLogin />} />
          <Route path="/oauth/callback"  element={<OAuthCallback />} />
          {/* Shared (any authenticated user) */}
          <Route path="/home" element={
            <ProtectedRoute><Home /></ProtectedRoute>
          } />
          <Route path="/profile" element={
            <ProtectedRoute><Profile /></ProtectedRoute>
          } />
          <Route path="/reviews" element={
            <ProtectedRoute><Reviews /></ProtectedRoute>
          } />
          <Route path="/membership" element={
            <ProtectedRoute><Membership /></ProtectedRoute>
          } />

          {/* Buyer only */}
          <Route path="/marketplace" element={
            <ProtectedRoute role="buyer"><Marketplace /></ProtectedRoute>
          } />
          <Route path="/cart" element={
            <ProtectedRoute role="buyer"><Cart /></ProtectedRoute>
          } />
          <Route path="/orders" element={
            <ProtectedRoute role="buyer"><MyOrders /></ProtectedRoute>
          } />

          {/* Seller only */}
          <Route path="/listings" element={
            <ProtectedRoute role="seller"><MyListings /></ProtectedRoute>
          } />
          <Route path="/listings/new" element={
            <ProtectedRoute role="seller"><CreateListing /></ProtectedRoute>
          } />
          <Route path="/incoming" element={
            <ProtectedRoute role="seller"><IncomingOrders /></ProtectedRoute>
          } />

          {/* Catch-all */}
          <Route path="*" element={<Navigate to="/" replace />} />
        </Routes>
      </BrowserRouter>
    </AuthProvider>
  );
}
