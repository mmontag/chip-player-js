import React, { Fragment, PureComponent } from 'react';
import VirtualList from 'react-virtual-list';
import BrowseList from './BrowseList';

const ITEM_HEIGHT = 19; // should match --charH CSS variable
const ITEM_BUFFER = 10;

const mapToVirtualProps = (props, state) => {
  const { items, itemHeight } = props;
  const { firstItemIndex, lastItemIndex } = state;
  const visibleItems = lastItemIndex > -1 ? items.slice(firstItemIndex, lastItemIndex + 1) : [];

  // style
  const height = items.length * itemHeight;
  const paddingTop = firstItemIndex * itemHeight;

  return {
    ...props,
    virtual: {
      items: visibleItems,
      style: {
        paddingTop,
        minHeight: height,
        height: height,
        boxSizing: 'border-box',
      },
    },
  };
};

export default class Browse extends PureComponent {
  constructor(props) {
    super(props);

    this.navigate = this.navigate.bind(this);
    this.handleShufflePlay = this.handleShufflePlay.bind(this);
    this.saveScrollPosition = this.saveScrollPosition.bind(this);
  }

  componentDidMount() {
    this.navigate();
  }

  componentWillUnmount() {
    this.saveScrollPosition(this.props.locationKey);
  }

  saveScrollPosition(locationKey) {
    const scrollTop = Math.round(this.props.scrollContainerRef.current.scrollTop);
    sessionStorage.setItem(locationKey, scrollTop.toString());
    // console.log("Browse.storeScroll: %s stored at %s", locationKey, scrollTop);
  }

  componentWillUpdate(nextProps, nextState, nextContext) {
    if (nextProps.browsePath === this.props.browsePath) return;

    this.saveScrollPosition(this.props.locationKey);
  }

  componentDidUpdate() {
    this.navigate();

    /*
    PROBLEM:

    When ?play=... is appended to URL, location.key becomes orphaned (undefined).
    The React Router history object might need to be shimmed to make this param
    completely transparent to browser navigation.
    See https://stackoverflow.com/a/56823112/264970.

    Consequences of trying to make ?play=... orthogonal to browse history:

    browse/b
      [click link]
    browse/b/c
      [play Song_X]
    browse/b/c?play=Song_X
      [scroll down]
      [back]
    browse/b
      [next song]
    browse/b?play=Song_Y (now location.key will be undefined)
      [forward]
    browse/b/c?play=Song_X (play param incorrectly reverts to Song_X)

    using back and forward has always been problematic with the play param,
    since it restores invalid URLs. Only now it breaks scroll restoration too.

    ...therefore, play param is now disabled.
     */
    if (this.props.historyAction === "POP") { // User has just navigated back or forward
      const { browsePath, locationKey } = this.props;
      if (sessionStorage.getItem(locationKey)) {
        const scrollToPosY = sessionStorage.getItem(locationKey);
        this.props.scrollContainerRef.current.scrollTo(0, scrollToPosY);
        console.debug("%s (%s) scroll position restored to %s", browsePath, locationKey, scrollToPosY);
      }
    }
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

  VirtualDirectoryListing = VirtualList({
    container: this.props.scrollContainerRef.current,
  }, mapToVirtualProps)(BrowseList);

  render() {
    const {
      listing,
      browsePath,
      playContext,
    } = this.props;
    const listingWithParent = [{
      path: '..',
      type: 'directory',
    }, ...(listing || [])];

    return (
      <Fragment>
        <div className="Browse-topRow">
          /{browsePath}{' '}
          <button
            className="box-button"
            title="Shuffle this directory (and all subdirectories)"
            onClick={this.handleShufflePlay}>
            Shuffle Play
          </button>
        </div>
        <this.VirtualDirectoryListing
          key={browsePath}
          {...this.props}
          playContext={playContext}
          items={listingWithParent}
          itemHeight={ITEM_HEIGHT}
          itemBuffer={ITEM_BUFFER}
        />
      </Fragment>
    );
  }
}
