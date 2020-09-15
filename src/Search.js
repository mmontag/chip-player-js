/* eslint import/no-webpack-loader-syntax: off */
import React, {Fragment, PureComponent} from 'react';
import queryString from 'querystring';
import FavoriteButton from "./FavoriteButton";
import debounce from 'lodash/debounce';
import promisify from "./promisifyXhr";
import {API_BASE, CATALOG_PREFIX} from "./config";
import DirectoryLink from "./DirectoryLink";
import {updateQueryString} from './util';

const MAX_RESULTS = 100;

function getTotal() {
  return fetch(`${API_BASE}/total`)
    .then(response => response.json());
}

export default class Search extends PureComponent {
  constructor(props) {
    super(props);

    this.doSearch = this.doSearch.bind(this);
    this.debouncedDoSearch = debounce(this.doSearch, 150);
    this.onChange = this.onChange.bind(this);
    this.onSearchInputChange = this.onSearchInputChange.bind(this);
    this.handleSearchResults = this.handleSearchResults.bind(this);
    this.handleClear = this.handleClear.bind(this);
    this.renderResultItem = this.renderResultItem.bind(this);

    this.textInput = React.createRef();

    this.state = {
      searching: false,
      results: {},
      resultsCount: 0,
      totalSongs: 0,
      query: null,
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
    updateQueryString({ q: val ? val.trim() : undefined });
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
    const q = encodeURIComponent(val);
    const url = `${API_BASE}/search?query=${q}&limit=${MAX_RESULTS}`;
    if (this.searchRequest) this.searchRequest.abort();
    this.searchRequest = promisify(new XMLHttpRequest());
    this.searchRequest.open('GET', url);
    // this.searchRequest.responseType = 'json';
    this.searchRequest.send()
      .then(response => {
        this.searchRequest = null;
        return JSON.parse(response.responseText);
      })
      .then(json => this.handleSearchResults(json));
  }

  handleSearchResults(payload) {
    const { items, total } = payload;
    const results = items
      .map(item => item.file)
      .map(result => result.replace('%', '%25').replace('#', '%23'))
      .sort();
    this.setState({
      searching: true,
      resultsCount: total,
      results: results,
      resultsHeadings: this.extractHeadings(results),
    });
  }

  handleClear() {
    const urlParams = queryString.parse(window.location.search.substr(1));
    delete urlParams.q;
    const search = queryString.stringify(urlParams);
    window.history.replaceState(null, '', search ? `?${search}` : './');
    this.setState({
      query: null,
      searching: false,
      resultsCount: 0,
      results: {}
    });
    this.textInput.current.focus();
  }

  showEmptyState() {
    this.setState({searching: false, results: {}})
  }

  extractHeadings(sortedResults) {
    // Results input must be sorted. Returns a map of indexes to headings.
    // {
    //   0: 'Nintendo/A Boy And His Blob',
    //   12: 'Sega Genesis/A Boy And His Blob',
    // }
    const headings = {};
    let currHeading = null;
    sortedResults.forEach((result, i) => {
      const heading = result.substring(0, result.lastIndexOf('/') + 1);
      if (heading !== currHeading) {
        currHeading = heading;
        headings[i] = currHeading;
      }
    });
    return headings;
  }

  renderResultItem(result, i) {
    let headingFragment = null;
    if (this.state.resultsHeadings[i]) {
      const href = this.state.resultsHeadings[i];
      headingFragment = (
        <DirectoryLink dim to={'/browse/' + href}>{decodeURI(href)}</DirectoryLink>
      );
    }
    const href = CATALOG_PREFIX + result;
    const resultTitle = decodeURI(result.substring(result.lastIndexOf('/') + 1));
    const isPlaying = this.props.currContext === this.state.results && this.props.currIdx === i;
    return (
      <Fragment key={i}>
        {headingFragment}
        <div className={isPlaying ? 'Song-now-playing' : '' }>
          {this.props.favorites &&
          <FavoriteButton favorites={this.props.favorites}
                          toggleFavorite={this.props.toggleFavorite}
                          href={href}/>}
          <a onClick={this.props.onSongClick(href, this.state.results, i)}
             href={href}>
            {resultTitle}
          </a>
        </div>
      </Fragment>
    );
  }

  render() {
    const placeholder = this.state.totalSongs ?
      `${this.state.totalSongs} tunes` : 'Loading catalog...';
    return (
      <Fragment>
        <div>
          <label className="Search-label">Search:{' '}
            <input type="text"
                   placeholder={placeholder}
                   spellCheck="false"
                   autoComplete="off"
                   autoCorrect="false"
                   autoCapitalize="none"
                   ref={this.textInput}
                   style={css.textInput}
                   value={this.state.totalSongs ? this.state.query || '' : ''}
                   onChange={this.onChange}/>
            {
              this.state.searching &&
              <Fragment>
                <button className="Search-button-clear" onClick={this.handleClear}/>
                {' '}
                <span style={css.resultsLabel}>
                  {this.state.resultsCount} result{this.state.resultsCount !== 1 && 's'}
                </span>
              </Fragment>
            }
          </label>
        </div>
        {
          this.state.searching ?
            <div style={css.searchResults}>
              {this.state.results.map(this.renderResultItem)}
            </div>
            :
            this.props.children
        }
      </Fragment>
    );
  }
}

const css = {
  searchResults: {
    marginTop: 'var(--charH)',
  },
  resultsLabel: { whiteSpace: 'nowrap' },
  textInput: {
    width: 'calc(var(--charW) * 20)',
  }
};
