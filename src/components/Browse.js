import React, { Fragment } from 'react';
import autoBindReact from 'auto-bind/react';
import VirtualizedList from './VirtualizedList';
import DirectoryLink from './DirectoryLink';
import bytes from 'bytes';
import { CATALOG_PREFIX } from '../config';
import FavoriteButton from './FavoriteButton';
import trimEnd from 'lodash/trimEnd';
import queryString from 'querystring';
import { pathJoin } from '../util';


export default class Browse extends React.PureComponent {
  constructor(props) {
    super(props);
    autoBindReact(this);
  }

  componentDidMount() {
    this.navigate();
  }

  componentDidUpdate(prevProps, prevState) {
    this.navigate();
  }

  handleShufflePlay() {
    this.props.handleShufflePlay(this.props.browsePath);
  }

  navigate() {
    const {
      browsePath,
      listing,
      fetchDirectory,
    } = this.props;
    if (!listing) {
      fetchDirectory(browsePath);
    }
  }

  render() {
    const {
      listing,
      browsePath,
      playContext,
      history,
    } = this.props;

    const urlParams = queryString.parse(window.location.search.slice(1));
    delete urlParams.q;
    const search = queryString.stringify(urlParams);
    // Check if previous page url is the parent directory of current page url.
    const prevPath = trimEnd(history.location.state?.prevPathname, '/');
    const currPath = trimEnd(window.location.pathname, '/');
    console.log("prevPath: %s\ncurrPath: %s", prevPath, currPath);
    const prevPageIsParentDir = prevPath === currPath.slice(0, currPath.lastIndexOf('/'));

    const BrowseRow = (props) => {
      const { item, onPlay } = props;
      item.isBackLink = item.name === '..' && prevPageIsParentDir;

      if (item.type === 'directory') {
        return (
          <>
            <div className="BrowseList-colName">
              <DirectoryLink to={pathJoin('/browse', item.path)} search={search}
                             isBackLink={item.isBackLink}>{item.name}</DirectoryLink>
            </div>
            <div className="BrowseList-colDir">
              &lt;DIR&gt;
            </div>
            <div className="BrowseList-colCount" title={`Contains ${item.numChildren} direct child items`}>
              {item.numChildren}
            </div>
            <div className="BrowseList-colMtime">
              {item.mtime}
            </div>
            <div className="BrowseList-colSize" title={`Directory size is ${item.size} bytes (recursive)`}>
              {item.size != null && bytes(item.size, { unitSeparator: ' ' })}
            </div>
          </>
        );
      } else {
        const href = pathJoin(CATALOG_PREFIX, item.path);
        return (
          <>
            <div className="BrowseList-colName">
              <FavoriteButton href={href}/>
              <a onClick={onPlay}
                 href={href}
                 tabIndex="-1">
                {item.name}
              </a>
            </div>
            <div className="BrowseList-colMtime">
              {item.mtime}
            </div>
            <div className="BrowseList-colSize">
              {bytes(item.size, { unitSeparator: ' ' })}
            </div>
          </>
        );
      }
    }

    return (
      <VirtualizedList
        scrollContainerRef={this.props.scrollContainerRef}
        currContext={this.props.currContext}
        currIdx={this.props.currIdx}
        onSongClick={this.props.onSongClick}
        itemList={listing || []}
        songContext={playContext}
        rowRenderer={BrowseRow}
        listRef={this.props.listRef}
        isSorted={true}
      >
        <h3 className="Browse-topRow">
          /{browsePath}{' '}
          <button
            className="box-button"
            title="Shuffle this directory (and all subdirectories)"
            onClick={this.handleShufflePlay}>
            Shuffle Play
          </button>
        </h3>
      </VirtualizedList>
    );
  }
}
