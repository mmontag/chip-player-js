import React from 'react';
import ReactDOM from 'react-dom';
import './index.css';
import App from './components/App';
import { BrowserRouter as Router } from 'react-router-dom';
import { UserProvider } from './components/UserProvider';
import { ToastProvider } from './components/ToastProvider';
import ThemeHandler from './components/ThemeHandler';

ReactDOM.render((
  <Router basename={process.env.PUBLIC_URL}>
    <ToastProvider>
      <UserProvider>
        <ThemeHandler/>
        <App/>
      </UserProvider>
    </ToastProvider>
  </Router>
), document.getElementById('root'));
