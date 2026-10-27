import React, { Fragment } from 'react';
import autoBindReact from 'auto-bind/react';
import VirtualizedList from './VirtualizedList';
import DirectoryLink from './DirectoryLink';
import bytes from 'bytes';
import FavoriteButton from './FavoriteButton';
import trimEnd from 'lodash/trimEnd';


export default class Browse extends React.PureComponent {
  constructor(props) {
    super(props);
    autoBindReact(this);
    this.state = { failedPath: null };
    this.pendingPaths = new Set();
  }

  componentDidMount() {
    this.mounted = true;
    window.addEventListener('online', this.retry);
    this.navigate();
  }

  componentWillUnmount() {
    this.mounted = false;
    window.removeEventListener('online', this.retry);
  }

  componentDidUpdate(prevProps, prevState) {
    this.navigate();
  }

  handleShufflePlay() {
    this.props.handleShufflePlay(this.props.browsePath);
  }

  retry() {
    if (this.state.failedPath !== this.props.browsePath) return;
    this.setState({ failedPath: null }, this.navigate);
  }

  navigate() {
    const {
      browsePath,
      listing,
      fetchDirectory,
    } = this.props;
    if (!listing && !this.pendingPaths.has(browsePath) && this.state.failedPath !== browsePath) {
      this.pendingPaths.add(browsePath);
      fetchDirectory(browsePath).catch(e => {
        console.warn('Could not load directory:', e);
        if (this.mounted && this.props.browsePath === browsePath) {
          this.setState({ failedPath: browsePath });
        }
      }).finally(() => this.pendingPaths.delete(browsePath));
    }
  }

  render() {
    const {
      listing,
      browsePath,
      playContext,
      history,
    } = this.props;

    const urlParams = new URLSearchParams(window.location.search);
    urlParams.delete('q');
    const search = urlParams.toString();
    // Check if previous page url is the parent directory of current page url.
    const prevPath = trimEnd(history.location.state?.prevPathname, '/');
    const currPath = trimEnd(window.location.pathname, '/');
    const prevPageIsParentDir = prevPath === currPath.slice(0, currPath.lastIndexOf('/'));

    const BrowseRow = (props) => {
      const { item, onPlay } = props;
      item.isBackLink = item.name === '..' && prevPageIsParentDir;

      if (item.type === 'directory') {
        return (
          <>
            <div className="BrowseList-colName">
              <DirectoryLink to={item.href} search={search}
                             isBackLink={item.isBackLink}>{item.name}</DirectoryLink>
            </div>
            <div className="BrowseList-colDir">
              &lt;DIR&gt;
            </div>
            <div className="BrowseList-colCount" title={`Contains ${item.count} direct child items`}>
              {item.count}
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
        return (
          <>
            <div className="BrowseList-colName">
              <FavoriteButton item={item}/>
              <a onClick={onPlay}
                 href={item.url}
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
        {this.state.failedPath === browsePath &&
          <p>Could not load this directory. Check your connection.{' '}
            <button className="box-button" onClick={this.retry}>Retry</button>
          </p>}
      </VirtualizedList>
    );
  }
}
