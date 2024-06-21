/* eslint import/no-webpack-loader-syntax: off */
import React, { Fragment, PureComponent } from 'react';
import queryString from 'querystring';
import debounce from 'lodash/debounce';
import { API_BASE, CATALOG_PREFIX } from '../config';
import promisify from '../promisify-xhr';
import { updateQueryString } from '../util';
import DirectoryLink from './DirectoryLink';
import FavoriteButton from './FavoriteButton';
import autoBindReact from 'auto-bind/react';

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
          this.searchRequest = null;
          return JSON.parse(response.responseText);
        })
        .then((payload) => {
          const { items, total } = payload;
          const results = items
            .map(item => item.file)
            .map(result => result.replace('%', '%25').replace('#', '%23'));
          searchResultsCache[val] = {
            resultsCount: total,
            resultsHeadings: this.extractHeadings(results),
            results: results,
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
    const { currContext, currIdx, onSongClick } = this.props;
    const href = CATALOG_PREFIX + result;
    const resultTitle = decodeURI(result.substring(result.lastIndexOf('/') + 1));
    const isPlaying = currContext === this.state.results && currIdx === i;
    return (
      <Fragment key={i}>
        {headingFragment}
        <div className={isPlaying ? 'Song-now-playing' : '' }>
          <FavoriteButton href={href}/>
          <a onClick={onSongClick(href, this.state.results, i)}
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
        </div>
        {
          this.state.searching ?
            <div className="Search-results">
              {this.state.results.map(this.renderResultItem)}
            </div>
            :
            this.props.children
        }
      </Fragment>
    );
  }
}
