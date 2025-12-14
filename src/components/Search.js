/* eslint import/no-webpack-loader-syntax: off */
import React, { Fragment, PureComponent } from 'react';
import queryString from 'querystring';
import debounce from 'lodash/debounce';
import { API_BASE, CATALOG_PREFIX } from '../config';
import promisify from '../promisify-xhr';
import { pathJoin } from '../util';
import DirectoryLink from './DirectoryLink';
import FavoriteButton from './FavoriteButton';
import autoBindReact from 'auto-bind/react';
import VirtualizedList from './VirtualizedList';

const MAX_RESULTS = 100;
const searchResultsCache = {};

function getTotal() {
  return fetch(`${API_BASE}/total`)
    .then(response => response.json());
}

export default class Search extends PureComponent {
  constructor(props) {
    super(props);
    autoBindReact(this);

    this.debouncedDoSearch = debounce(this.doSearch, 150);

    this.textInput = React.createRef();

    this.state = {
      totalSongs: 0,
      query: null,
      searching: false,

      results: [],
      resultsContext: [],
      resultsCount: 0,
    };

    getTotal()
      .then(json => this.setState({ totalSongs: json.total }))
      .catch(_ => this.setState({ totalSongs: 99999 }));
  }

  componentDidMount() {
    const {q} = queryString.parse(window.location.search.substr(1));
    if (q) {
      this.setState({query: q});
      this.doSearch(q);
    }
  }

  onChange(e) {
    this.onSearchInputChange(e.target.value);
  }

  onSearchInputChange(val, immediate = false) {
    this.setState({query: val});
    // updateQueryString({ q: val ? val.trim() : undefined });
    const newParams = { q: val ? val.trim() : undefined };
    // Merge new params with current query string
    const params = {
      ...queryString.parse(window.location.search.substr(1)),
      ...newParams,
    };
    // Delete undefined properties
    Object.keys(params).forEach(key => params[key] === undefined && delete params[key]);
    // Object.keys(params).forEach(key => params[key] = decodeURIComponent(params[key]));
    const stateUrl = '?' + queryString.stringify(params).replace(/%20/g, '+');
    // Update address bar URL
    this.props.history.replace(stateUrl);

    if (val.length) {
      if (immediate) {
        this.doSearch(val);
      } else {
        this.debouncedDoSearch(val);
      }
    } else {
      this.showEmptyState();
    }
  }

  doSearch(val) {
    if (searchResultsCache[val]) {
      this.setState({
        searching: true,
        ...searchResultsCache[val],
      });
    } else {
      const q = encodeURIComponent(val);
      const url = `${API_BASE}/search?query=${q}&limit=${MAX_RESULTS}`;
      if (this.searchRequest) this.searchRequest.abort();
      this.searchRequest = promisify(new XMLHttpRequest());
      this.searchRequest.open('GET', url);
      this.searchRequest.send()
        .then(response => {
          /*
          Example search response:
          {
            "items": [
              {
                "id": 609,
                "file": "Classical MIDI/Bach/Bwv0565 Toccata and Fugue In Dm A.mid",
                "depth": 3
              },
              {
                "id": 677,
                "file": "Classical MIDI/Bach/Bwv1046 aSinfonia h.mid",
                "depth": 3
              },
            ],
            "total": 2
          }
          */
          this.searchRequest = null;
          return JSON.parse(response.responseText);
        })
        .then((payload) => {
          const { items, total } = payload;
          // Decorate file items with idx (to match up with song context) and other properties.
          const resultFiles = items
            .map((item, i) => {
              const path = item.file;
              return {
                idx: i,
                path: path,
                name: path.substring(path.lastIndexOf('/') + 1),
                href: pathJoin(CATALOG_PREFIX, path.replace('%', '%25').replace('#', '%23')),
                type: 'file',
              };
            });
          const resultsContext = resultFiles.map(item => item.href);
          // Build the results list with interleaved directory headings.
          const resultsWithHeadings = [];
          let currHeading = null;
          resultFiles.forEach((item) => {
            const { path } = item;
            const heading = path.substring(0, path.lastIndexOf('/') + 1);
            if (heading !== currHeading) {
              currHeading = heading;
              resultsWithHeadings.push({
                type: 'directory',
                href: pathJoin('/browse', heading),
                name: heading,
              });
            }
            resultsWithHeadings.push(item);
          });
          // Cache the computed results for this query.
          searchResultsCache[val] = {
            resultsCount: total,
            resultsContext: resultsContext,
            results: resultsWithHeadings,
          };
          this.setState({
            searching: true,
            ...searchResultsCache[val],
          });
        });
    }
  }

  handleClear() {
    const urlParams = queryString.parse(window.location.search.substr(1));
    delete urlParams.q;
    const search = queryString.stringify(urlParams);
    window.history.replaceState(null, '', search ? `?${search}` : './');
    this.setState({
      query: null,
      searching: false,
      results: [],
      resultsContext: [],
      resultsCount: 0,
    });
    this.textInput.current.focus();
  }

  showEmptyState() {
    this.setState({searching: false, results: []})
  }

  renderResultItem(props) {
    const { item, onPlay } = props;
    if (item.type === 'directory') {
      return (
        <DirectoryLink dim to={item.href}>{item.name}</DirectoryLink>
      );
    } else {
      return (
        <>
          <FavoriteButton href={item.href}/>
          <a onClick={onPlay} href={item.href} tabIndex="-1">{item.name}</a>
        </>
      );
    }
  }

  render() {
    const placeholder = this.state.totalSongs ?
      `${this.state.totalSongs} tunes` : 'Loading catalog...';

    const {
      onSongClick,
      currContext,
      currIdx,
      scrollContainerRef,
      listRef,
    } = this.props;

    return (
        <>
          <VirtualizedList
            currContext={currContext}
            currIdx={currIdx}
            onSongClick={onSongClick}
            itemList={this.state.results}
            songContext={this.state.resultsContext}
            rowRenderer={this.renderResultItem}
            isSorted={false}
            scrollContainerRef={scrollContainerRef}
            listRef={listRef}
          >
            <h3 className="Browse-topRow">
              <label className="Search-label">Search:{' '}
                <input type="text"
                       placeholder={placeholder}
                       spellCheck="false"
                       autoComplete="off"
                       autoCorrect="false"
                       autoCapitalize="none"
                       autoFocus
                       ref={this.textInput}
                       className="Search-input"
                       value={this.state.totalSongs ? this.state.query || '' : ''}
                       onChange={this.onChange}/>
                {
                  this.state.searching &&
                  <Fragment>
                    <button className="Search-clearButton" onClick={this.handleClear}/>
                    {' '}
                    <span className="Search-resultsLabel">
                        {this.state.resultsCount} result{this.state.resultsCount !== 1 && 's'}
                      </span>
                  </Fragment>
                }
              </label>
            </h3>
          </VirtualizedList>
          {this.state.searching || this.props.children}
        </>
    );
  }
}
