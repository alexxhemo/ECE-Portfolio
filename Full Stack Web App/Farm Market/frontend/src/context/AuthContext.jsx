import React, { createContext, useContext, useState, useEffect } from 'react';
import { authApi } from '../api';

const AuthContext = createContext(null);

export function AuthProvider({ children }) {
  const [user, setUser]       = useState(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const token = localStorage.getItem('fm_token');
    if (token) {
      authApi.me()
        .then(data => setUser(data))
        .catch(() => localStorage.removeItem('fm_token'))
        .finally(() => setLoading(false));
    } else {
      setLoading(false);
    }
  }, []);

  const login = async (credentials) => {
    const data = await authApi.login(credentials);
    localStorage.setItem('fm_token', data.token);
    setUser(data.user);
    return data.user;
  };

  const register = async (formData) => {
    const data = await authApi.register(formData);
    localStorage.setItem('fm_token', data.token);
    setUser(data.user);
    return data.user;
  };

    const completeOAuthLogin = async (token) => {
    localStorage.setItem('fm_token', token);
    const data = await authApi.me();
    setUser(data);
    return data;
  };

  const logout = () => {
    localStorage.removeItem('fm_token');
    setUser(null);
  };

  const refreshUser = async () => {
    const data = await authApi.me();
    setUser(data);
  };

  return (
    <AuthContext.Provider value={{ user, loading, login, register, completeOAuthLogin, logout, refreshUser }}>
      {children}
    </AuthContext.Provider>
  );
}

export const useAuth = () => useContext(AuthContext);
