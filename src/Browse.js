import React, {Fragment, PureComponent} from 'react';
import bytes from 'bytes';
import FavoriteButton from "./FavoriteButton";
import {CATALOG_PREFIX} from "./config";
import {DirectoryLink} from "./DirectoryLink";

const dirToken = <Fragment>&nbsp;&nbsp;&lt;DIR&gt;&nbsp;&nbsp;</Fragment>;

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
      this.contexts[browsePath] = directories[browsePath].map(item => item.path);
    }
  }

  render() {
    const {
      directories,
      currContext,
      currIdx,
      favorites,
      toggleFavorite,
      handleSongClick,
      browsePath,
    } = this.props;
    const dirListing = directories[browsePath] || [];
    const parentPath = browsePath.substr(0, browsePath.lastIndexOf('/'));

    return (
      <Fragment>
      <div>
        /{browsePath}
      </div>
      <table style={css.directoryTable}>
        <tbody>
        <tr key={browsePath}>
            <td>
              <DirectoryLink to={'/browse/' + parentPath}>..</DirectoryLink>
            </td>
            <td>{dirToken}</td>
            <td></td>
          </tr>
        {
          dirListing.map((item, i) => {
            // XXX: Escape immediately: the escaped URL is considered canonical.
            //      The URL must be decoded for display from here on out.
            const path = item.path.replace('%', '%25').replace('#', '%23');
            const name = item.path.split('/').pop();
            const isPlaying = currContext === this.contexts[browsePath] && currIdx === i;
            if (item.type === 'directory') {
              return (
                <tr key={name}>
                  <td style={css.tdClip}>
                    <div style={css.textClip}>
                      <DirectoryLink to={'/browse' + path}>{name}</DirectoryLink>
                    </div>
                  </td>
                  <td>
                    {dirToken}
                  </td>
                  <td style={css.rightAlign}>
                    {bytes(item.size, {unitSeparator: ' '})}
                  </td>
                </tr>
              );
            } else {
              const href = CATALOG_PREFIX + path;
              return (
                <tr key={name}>
                  <td style={css.tdClip}>
                    <div style={css.textClip}>
                      {favorites &&
                      <FavoriteButton favorites={favorites}
                                      href={href}
                                      toggleFavorite={toggleFavorite}/>}
                      <a className={isPlaying ? 'Song-now-playing' : ''}
                         onClick={handleSongClick(href, this.contexts[browsePath], i)}
                         href='#'>
                        {name}
                      </a>
                    </div>
                  </td>
                  <td>
                  </td>
                  <td style={css.rightAlign}>
                    {bytes(item.size, {unitSeparator: ' '})}
                  </td>
                </tr>
              );
            }
          })
        }
        </tbody>
      </table>
      </Fragment>
    );
  }
}

const css = {
  directoryTable: {
    flexGrow: 1,
    borderSpacing: 0,
    tableLayout: 'fixed',
  },
  rightAlign: {
    textAlign: 'right',
    textTransform: 'uppercase',
    whiteSpace: 'nowrap',
  },
  tdClip: {
    width: '100%',
    position: 'relative',
    padding: 0,
  },
  textClip: {
    overflowX: 'hidden',
    whiteSpace: 'nowrap',
    textOverflow: 'ellipsis',
    position: 'absolute',
    top: 0,
    left: 0,
    right: 0,
  },
};
