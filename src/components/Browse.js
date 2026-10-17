import React, { Fragment } from 'react';
import autoBindReact from 'auto-bind/react';
import VirtualizedList from './VirtualizedList';
import DirectoryLink from './DirectoryLink';
import bytes from 'bytes';
import FavoriteButton from './FavoriteButton';
import trimEnd from 'lodash/trimEnd';
import { isSongFolderListing } from '../util';


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

    const urlParams = new URLSearchParams(window.location.search);
    urlParams.delete('q');
    const search = urlParams.toString();
    // Rows of a song folder are its songs; a directory's rows are not.
    const songFolder = isSongFolderListing(listing);
    // Check if previous page url is the parent directory of current page url.
    const prevPath = trimEnd(history.location.state?.prevPathname, '/');
    const currPath = trimEnd(window.location.pathname, '/');
    const prevPageIsParentDir = prevPath === currPath.slice(0, currPath.lastIndexOf('/'));

    const BrowseRow = (props) => {
      const { item, onPlay } = props;
      item.isBackLink = item.name === '..' && prevPageIsParentDir;

      const isSongFolder = item.type === 'songfolder';
      if (item.type === 'directory' || isSongFolder) {
        return (
          <>
            <div className="BrowseList-colName">
              <DirectoryLink to={item.href} search={search}
                             isBackLink={item.isBackLink}>{item.name}</DirectoryLink>
            </div>
            <div className="BrowseList-colDir">
              {isSongFolder ? <>&lt;TUNES&gt;</> : <>&lt;DIR&gt;</>}
            </div>
            <div className="BrowseList-colCount"
                 title={isSongFolder ? `Contains ${item.count} sub-tunes` : `Contains ${item.count} direct child items`}>
              {item.count}
            </div>
            <div className="BrowseList-colMtime">
              {item.mtime}
            </div>
            <div className="BrowseList-colSize"
                 title={isSongFolder ? `File size is ${item.size} bytes` : `Directory size is ${item.size} bytes (recursive)`}>
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
            title={songFolder
              ? 'Shuffle this song folder'
              : 'Shuffle this directory (and all subdirectories)'}
            onClick={this.handleShufflePlay}>
            Shuffle Play
          </button>
        </h3>
      </VirtualizedList>
    );
  }
}
