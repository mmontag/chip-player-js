import React from "react";
import { NavLink } from 'react-router-dom';

export default class AppHeader extends React.PureComponent {
  render() {
    return (
      <header className="AppHeader">
        <NavLink className="AppHeader-title" to={{ pathname: "/" }}>Chip Player JS</NavLink>{' '}
        {this.props.user ?
          <span>
              • Logged in as {this.props.user.displayName}.{' '}
            <a href="#" onClick={this.props.handleLogout}>Logout</a>
            </span>
          :
          <span>
              • <a href="#" onClick={this.props.handleLogin}>Login/Sign Up</a> to Save Favorites
            </span>
        }
        {!this.props.isPhone &&
        <p className="AppHeader-subtitle">
            <span className="AppHeader-byline">Feedback:{' '}
              <a href="https://twitter.com/matthewmontag" target="_blank" rel="noopener noreferrer">
                @matthewmontag
              </a>
            </span>
          Powered by{' '}
          <a href="https://bitbucket.org/mpyne/game-music-emu/wiki/Home">Game Music Emu</a>,{' '}
          <a href="https://github.com/cmatsuoka/libxmp">LibXMP</a>, and{' '}
          <a href="https://github.com/divideconcept/FluidLite">FluidLite</a>.
        </p>}
      </header>
    );
  }
}
