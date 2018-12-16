import React, {PureComponent} from "react";
import FavoriteButton from "./FavoriteButton";

export default class Favorites extends PureComponent {
  render() {
    return (<div>
      <h3>My Favorites</h3>
      {this.props.favorites ?
        Object.keys(this.props.favorites).map((href, i) => {
            const title = decodeURIComponent(href.split('/').pop());
            return (
              <div className="Search-results-group-item" key={i}>
                <FavoriteButton favorites={this.props.favorites}
                                toggleFavorite={this.props.toggleFavorite}
                                href={href}/>
                <a onClick={this.props.onClick(href)} href={href}>{title}</a>
              </div>
            );
          }
        )
        :
        "(No favorites)"
      }
    </div>);
  }
}