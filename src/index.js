import React from 'react';
import ReactDOM from 'react-dom';
import './index.css';
import App from './components/App';
import { BrowserRouter as Router } from 'react-router-dom';
import { UserProvider } from './components/UserProvider';
import { ToastProvider } from './components/ToastProvider';

ReactDOM.render((
  <Router basename={process.env.PUBLIC_URL}>
    <ToastProvider>
      <UserProvider>
        <App/>
      </UserProvider>
    </ToastProvider>
  </Router>
), document.getElementById('root'));

// check the 'theme' param in the URL and set the theme accordingly
const urlParams = new URLSearchParams(window.location.search);
const theme = urlParams.get('theme');
if (theme) {
  // This maps to CSS selector `html[data-theme=...]`
  document.documentElement.setAttribute('data-theme', theme);
} else {
  document.documentElement.setAttribute('data-theme', 'msdos');
}
