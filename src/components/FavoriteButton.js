import React, { PureComponent } from "react";
import autoBindReact from 'auto-bind/react';

export default class FavoriteButton extends PureComponent {
  constructor(props) {
    super(props);
    autoBindReact(this);
  }

  handleClick() {
    this.props.toggleFavorite(this.props.href);
  }

  render() {
    const { isFavorite } = this.props;
    return (
      <button onClick={this.handleClick}
              className={'Favorite-button' + (isFavorite ? ' isFavorite' : '')}>
        &hearts;
      </button>
    );
  }
}
