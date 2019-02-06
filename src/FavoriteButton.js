import React, {PureComponent} from "react";

export default class FavoriteButton extends PureComponent {
  constructor(props) {
    super(props);
    this.handleClick = this.handleClick.bind(this);
  }

  handleClick() {
    this.props.toggleFavorite(this.props.href);
  }

  render() {
    const added = !!this.props.favorites[this.props.href];
    return (
      <button onClick={this.handleClick}
              className={'Favorite-button' + (added ? ' added' : '')}>
        &#003;
      </button>
    );
  }
}