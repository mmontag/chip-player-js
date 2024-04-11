import React from "react";
import { Link } from 'react-router-dom';

export default class AppHeader extends React.PureComponent {
  render() {
    return (
      <header className="AppHeader">
        <Link className="AppHeader-title" to={{ pathname: "/" }}>Chip Player JS</Link>
        {this.props.user ?
          <>
            {' • '}
            Logged in as {this.props.user.displayName}.
            {' '}
            <a href="#" onClick={this.props.handleLogout}>Logout</a>
          </>
          :
          <>
            {' • '}
            <a href="#" onClick={this.props.handleLogin}>Login/Sign Up</a> to Save Favorites
          </>
        }
        {' • '}
        <a href="https://twitter.com/messages/compose?recipient_id=587634572" target="_blank" rel="noopener noreferrer">
          Feedback
        </a>
      </header>
    );
  }
}
