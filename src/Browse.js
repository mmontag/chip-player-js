import React, {Fragment, PureComponent} from 'react';
import VirtualList from 'react-virtual-list';
import BrowseList from './BrowseList';

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

    this.contexts = {};
  }

  componentDidMount() {
    this.navigate();
  }

  componentDidUpdate() {
    this.navigate();
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
    const dirListing = directories[browsePath] || [];

    return (
      <Fragment>
        <div>
          /{browsePath}
        </div>
        <this.VirtualDirectoryListing
          key={browsePath}
          {...this.props}
          contexts={this.contexts}
          items={dirListing}
          itemHeight={19}
        />
      </Fragment>
    );
  }
}
