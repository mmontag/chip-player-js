/* eslint import/no-webpack-loader-syntax: off */
import React, { Fragment, PureComponent } from 'react';
import axios from 'redaxios';
import autoBindReact from 'auto-bind/react';
import debounce from 'lodash/debounce';
import queryString from 'querystring';

import { API_BASE } from '../config';
import { getUrlFromFilepath, pathJoin } from '../util';
import DirectoryLink from './DirectoryLink';
import FavoriteButton from './FavoriteButton';
import VirtualizedList from './VirtualizedList';

const MAX_RESULTS = 400;
const searchResultsCache = {};

export default class Search extends PureComponent {
  constructor(props) {
    super(props);
    autoBindReact(this);

    this.debouncedDoSearch = debounce(this.doSearch, 150);

    this.textInput = React.createRef();

    this.state = {
      totalSongs: 300000,
      query: null,
      searching: false,

      results: [],
      resultsContext: [],
      resultsCount: 0,
    };

    axios.get(`${API_BASE}/total`)
      .then(response => response.data)
      .then(json => this.setState({ totalSongs: json.total }));
  }

  componentDidMount() {
    const { q } = queryString.parse((this.props.location?.search.substr(1) || ''));
    if (q) {
      this.setState({ query: q });
      this.doSearch(q);
    }
  }

  // NOTE: This is not fully vetted
  componentDidUpdate(prevProps) {
    // If the route's query string changed (back/forward navigation), sync the input and trigger search.
    const prevSearch = prevProps.location?.search || '';
    const currSearch = this.props.location?.search || '';
    if (prevSearch !== currSearch) {
      const { q } = queryString.parse(currSearch.replace(/^\?/, ''));
      if (q) {
        if (q !== this.state.query) {
          this.setState({ query: q });
          this.doSearch(q);
        }
      } else if (this.state.query) {
        // param removed -> clear
        this.setState({ query: null, searching: false, results: [], resultsContext: [], resultsCount: 0 });
      }
    }
  }

  onChange(e) {
    this.onSearchInputChange(e.target.value);
  }

  onSearchInputChange(val, immediate = false) {
    this.setState({query: val});
    // updateQueryString({ q: val ? val.trim() : undefined });
    const newParams = { q: val ? val : undefined };
    // Merge new params with current query string
    const params = {
      ...queryString.parse((this.props.location?.search.substr(1) || '')),
      ...newParams,
    };
    // Delete undefined properties
    Object.keys(params).forEach(key => params[key] === undefined && delete params[key]);
    // Object.keys(params).forEach(key => params[key] = decodeURIComponent(params[key]));
    const stateUrl = '?' + queryString.stringify(params).replace(/%20/g, '+');
    // Update address bar URL via react-router history
    this.props.history.replace({
      pathname: (this.props.location?.pathname) || '/',
      search: stateUrl
    });

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
      this.searchRequest = new AbortController();
      axios.get(url, {
          signal: this.searchRequest.signal
        })
        .then(response => {
          const { items, total } = response.data;
          // Decorate file items with idx (to match up with song context) and other properties.
          const resultFiles = items
            // Sort by full file path to group directories together
            .sort((a, b) => a.file.localeCompare(b.file))
            .map((item, i) => {
              const path = item.file;
              return {
                idx: i,
                path: path,
                name: path.substring(path.lastIndexOf('/') + 1),
                href: getUrlFromFilepath(path),
                type: 'file',
                songId: item.song_id,
              };
            });
          const resultsContext = resultFiles.map(item => item.path);
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
    const urlParams = queryString.parse((this.props.location?.search.substr(1) || ''));
    delete urlParams.q;
    const search = queryString.stringify(urlParams);
    this.props.history.replace({ pathname: (this.props.location?.pathname) || '/', search: search ? `?${search}` : '' });
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
          <FavoriteButton item={item}/>
          <a onClick={onPlay} href={item.href} tabIndex="-1">{item.name}</a>
        </>
      );
    }
  }

  render() {
    const placeholder = `${this.state.totalSongs} tunes`;

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
