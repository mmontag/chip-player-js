import React, {Fragment, PureComponent} from 'react';
import VirtualList from 'react-virtual-list';
import BrowseList from './BrowseList';

const ITEM_HEIGHT = 19; // should match --charH CSS variable
const ITEM_BUFFER = 10;
const VIRTUAL_LIST_POS_Y = 3 * ITEM_HEIGHT; // 3 static lines of text above the list in the scroll container

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

    this.contexts = {};
  }

  componentDidMount() {
    this.navigate();
  }

  componentDidUpdate() {
    this.navigate();
  }

  handleShufflePlay() {
    this.props.handleShufflePlay(this.props.browsePath);
  }

  navigate() {
    const browsePath = this.props.browsePath;
    const directories = this.props.directories;
    const fetchDirectory = this.props.fetchDirectory;
    if (!directories[browsePath]) {
      fetchDirectory(browsePath);
    }
    if (directories[browsePath] && !this.contexts[browsePath]) {
      this.contexts[browsePath] = directories[browsePath].map(item =>
        item.path.replace('%', '%25').replace('#', '%23').replace(/^\//, '')
      );
    }
  }

  VirtualDirectoryListing = VirtualList({
    container: this.props.scrollContainerRef.current,
  }, mapToVirtualProps)(BrowseList);

  render() {
    const {
      directories,
      browsePath,
    } = this.props;
    const listing = directories[browsePath] || [];
    const listingWithParent = [{
      path: '..',
      type: 'directory',
    }, ...listing];

    return (
      <Fragment>
        <div style={css.row}>
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
          contexts={this.contexts}
          items={listingWithParent}
          itemHeight={ITEM_HEIGHT}
          itemBuffer={ITEM_BUFFER}
        />
      </Fragment>
    );
  }
}

const css = {
  row: {
    display: 'flex',
    justifyContent: 'space-between',
    marginBottom: 'var(--charH)',
  },
};
