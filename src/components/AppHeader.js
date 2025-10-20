import React, { useContext } from 'react';
import { Link } from 'react-router-dom';
import { UserContext } from './UserProvider';

const AppHeader = () => {
  const { user, handleLogout, handleLogin } = useContext(UserContext);

  return (
    <header className="AppHeader">
      <Link className="AppHeader-title" to={{ pathname: "/" }}>Chip Player JS</Link>
      {user ?
        <>
          {' • '}
          Logged in as {user.displayName}.
          {' '}
          <a href="#" onClick={handleLogout}>Logout</a>
        </>
        :
        <>
          {' • '}
          <a href="#" onClick={handleLogin}>Login/Sign Up</a> to Save Favorites
        </>
      }
      {' • '}
      <a href="https://twitter.com/messages/compose?recipient_id=587634572" target="_blank" rel="noopener noreferrer">
        Feedback
      </a>
    </header>
  );
}

export default AppHeader;
