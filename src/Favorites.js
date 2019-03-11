import React, {PureComponent} from "react";
import FavoriteButton from "./FavoriteButton";

export default class Favorites extends PureComponent {
  constructor(props) {
    super(props);
    // this.favorites = props.favorites ? Object.keys(props.favorites) : [];
  }

  render() {
    // const favorites = this.props.favorites ? Object.keys(this.props.favorites) : [];
    const favorites = this.props.favorites;
    const currContext = this.props.currContext;
    const currIdx = this.props.currIdx;
    return (
      <div>
        <h3>
          My Favorites <span>({favorites.length})</span>
        </h3>
        {this.props.user ?
          favorites.length > 0 ?
            <div>
              {
                favorites.map((href, i) => {
                  const title = decodeURIComponent(href.split('/').pop());
                  const isPlaying = currContext === favorites && currIdx === i;
                  return (
                    <div className="Search-results-group-item" key={i}>
                      <FavoriteButton favorites={this.props.favorites}
                                      toggleFavorite={this.props.toggleFavorite}
                                      href={href}/>
                      <a className={isPlaying ? 'Song-now-playing' : ''}
                         onClick={this.props.onClick(href, favorites, i)} href={href}>{title}</a>
                    </div>
                  );
                })
              }
            </div>
            :
            <div>
              You don't have any favorites yet.<br/>
              Click the &#003; heart icon next to any song to save a favorite.
            </div>
          :
          <span>You must <a href="#" onClick={this.props.handleLogin}>login or signup</a> to save favorites.</span>
        }
      </div>
    );
  }
}