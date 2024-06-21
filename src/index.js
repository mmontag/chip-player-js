import React from 'react';
import ReactDOM from 'react-dom';
import './index.css';
import App from './components/App';
import { BrowserRouter as Router } from 'react-router-dom';
import { UserProvider } from './components/UserProvider';

ReactDOM.render((
  <Router basename={process.env.PUBLIC_URL}>
    <UserProvider>
      <App/>
    </UserProvider>
  </Router>
), document.getElementById('root'));
