import React, {PureComponent} from 'react';
import PropTypes from 'prop-types';
import prettyBytes from 'pretty-bytes';
import FavoriteButton from "./FavoriteButton";
import {CATALOG_PREFIX} from "./config";
import folder from './images/folder.png';

export default class Browse extends PureComponent {
  constructor(props) {
    super(props);

    this.handleDirectoryClick = this.handleDirectoryClick.bind(this);
    this.handleNavigate = this.handleNavigate.bind(this);

    this.state = {
      browsePath: '/',
    };

    this.contexts = {};
  }

  componentDidMount() {
    this.handleNavigate(this.state.browsePath);
  }

  componentDidUpdate() {
    const browsePath = this.state.browsePath;
    const directories = this.props.directories;
    if (directories[browsePath] && !this.contexts[browsePath]) {
      this.contexts[browsePath] = directories[browsePath].map(item => item.path);
    }
  }

  handleDirectoryClick(e, path) {
    e.preventDefault();
    this.handleNavigate(path);
  }

  handleNavigate(path) {
    this.setState({
      browsePath: path,
    });

    if (!this.props.directories[path]) {
      this.props.fetchDirectory(path);
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
    } = this.props;
    const {browsePath} = this.state;
    const dirListing = directories[browsePath] || [];

    return (
      <table style={css.directoryTable}><tbody>
        {
          dirListing.map((item, i) => {
            const isPlaying = currContext === this.contexts[browsePath] && currIdx === i;
            const name = item.path.split('/').pop();
            if (item.type === 'directory') {
              return (
                <tr key={name}>
                  <td style={css.tdClip}>
                    <div style={css.textClip}>
                    <a href={'?path=' + item.path}
                       onClick={(e) => this.handleDirectoryClick(e, item.path)}>
                       <img style={css.folderImg} src={folder}/>{name}
                    </a>
                    </div>
                  </td>
                  <td>
                    &nbsp;&nbsp;&lt;DIR&gt;&nbsp;&nbsp;
                  </td>
                  <td style={css.rightAlign}>
                    {prettyBytes(item.size)}
                  </td>
                </tr>
              );
            } else {
              // XXX: Escape immediately: the escaped URL is considered canonical.
              //      The URL must be decoded for display from here on out.
              const href = CATALOG_PREFIX + item.path.replace('%', '%25').replace('#', '%23');
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
                    {prettyBytes(item.size)}
                  </td>
                </tr>
              );
            }
          })
        }
      </tbody></table>
    );
  }
}

const css = {
  directoryTable: {
    flexGrow: 1,
    padding: '19px 16px',
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
  folderImg: {
    verticalAlign: 'bottom',
  },
};
