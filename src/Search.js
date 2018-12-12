/* eslint import/no-webpack-loader-syntax: off */
import React, {PureComponent} from 'react';
import SearchWorker from 'worker-loader!./SearchWorker';
import queryString from 'querystring';
import FavoriteButton from "./FavoriteButton";

const searchWorker = new SearchWorker();

const CATALOG_PREFIX = 'https://gifx.co/music/';
const MAX_RESULTS = 200;

export default class Search extends PureComponent {
  constructor(props) {
    super(props);

    this.doSearch = this.doSearch.bind(this);
    this.onChange = this.onChange.bind(this);
    this.onSearchInputChange = this.onSearchInputChange.bind(this);

    this.handleSearchResults = this.handleSearchResults.bind(this);
    this.handleGroupClick = this.handleGroupClick.bind(this);
    this.handleStatus = this.handleStatus.bind(this);

    searchWorker.onmessage = (message) => this.handleMessage(message.data);

    if (props.catalog) {
      this.loadCatalog(props.catalog);
    }

    this.state = {
      searching: false,
      results: {},
      resultsCount: 0,
      totalSongs: 0,
      query: null,
    }
  }

  componentDidUpdate() {
    if (!this.catalogLoaded && this.props.catalog) {
      this.loadCatalog(this.props.catalog);
    }
  }

  loadCatalog(catalog) {
    console.log('Posting catalog load message to worker...');
    this.catalogLoaded = true;
    searchWorker.postMessage({
      type: 'load',
      payload: JSON.stringify(catalog),
    });
  }

  onChange(e) {
    this.onSearchInputChange(e.target.value);
  }

  onSearchInputChange(val) {
    this.setState({query: val});
    const urlParams = {
      q: val ? val.trim() : undefined,
    };
    const stateUrl = '?' + queryString.stringify(urlParams).replace(/%20/g, '+');
    window.history.replaceState(null, '', stateUrl);
    if (val.length) {
      this.doSearch(val);
    } else {
      this.showEmptyState();
    }
  }

  doSearch(val) {
    const query = val.trim().split(' ').filter(n => n !== '');
    searchWorker.postMessage({
      type: 'search',
      payload: {
        query: query,
        maxResults: MAX_RESULTS
      }
    });
  }

  handleMessage(data) {
    switch (data.type) {
      case 'results':
        this.handleSearchResults(data.payload);
        break;
      case 'status':
        this.handleStatus(data.payload);
        break;
      default:
    }
  }

  handleStatus(data) {
    if (data.numRecords && this.props.initialQuery) {
      this.onSearchInputChange(this.props.initialQuery);
    }
    this.setState({
      totalSongs: data.numRecords,
    });
  }

  handleSearchResults(payload) {
    this.setState({
      searching: true,
      resultsCount: payload.count,
      results: this.groupResults(payload.results.map(result => result.file).sort()),
    });
  }

  showEmptyState() {
    this.setState({searching: false, results: {}})
  }

  groupResults(results) {
    // convert to nested results - one level deep
    const grouped = [];
    let current = {title: null, items: []};
    results.forEach(result => {
      const prefix = result.substring(0, result.lastIndexOf('/') + 1);
      const suffix = result.substring(result.lastIndexOf('/') + 1);
      if (prefix !== current.title) {
        current = {title: prefix, items: []};
        grouped.push(current);
      }
      current.items.push(suffix);
    });
    return grouped;
  }

  handleSearchFocus(e) {
    // highlight text
    e.target.select();
    // ignore mouseup if focused by click
    if (e.button) {
      e.target.onmouseup = () => {
        e.target.onmouseup = null;
        return false;
      }
    }
  };

  handleGroupClick(e, query) {
    e.preventDefault();
    this.onSearchInputChange(query);
  }

  render() {
    const placeholder = this.state.totalSongs ?
      `${this.state.totalSongs} tunes` : 'Loading catalog...';
    return (
      <div className="Search">
        <div>
          <label>Search: <input type="text"
                                placeholder={placeholder}
                                spellCheck="false"
                                autoComplete="off"
                                autoCorrect="false"
                                autoCapitalize="none"
                                value={this.state.totalSongs ? this.state.query || '' : ''}
                                onFocus={this.handleSearchFocus}
                                onChange={this.onChange}/></label>
          {
            this.state.searching &&
            <span>{this.state.resultsCount} result{this.state.resultsCount !== 1 && 's'}</span>
          }
        </div>
        {
          this.state.searching ?
            <div className="Search-results">
              {this.state.results.map((group, i) => {
                const groupQuery = group.title.replace(/[^a-zA-Z0-9]+/g, ' ');
                return (
                  <div key={i}>
                    <h5 className="Search-results-group-heading">
                      <a href={'?q=' + groupQuery} onClick={(e) => this.handleGroupClick(e, groupQuery)}>
                        {group.title}
                      </a>
                    </h5>
                    {group.items.map((result, i) => {
                      const href = CATALOG_PREFIX + group.title + result.replace('%', '%25').replace('#', '%23');
                      return (
                        <div className="Search-results-group-item" key={i}>
                          {this.props.favorites &&
                          <FavoriteButton favorites={this.props.favorites}
                                          toggleFavorite={this.props.toggleFavorite}
                                          href={href}/>}
                          <a onClick={this.props.onResultClick(href)} href={href}>{result}</a>
                        </div>
                      )
                    })}
                  </div>
                );
              })}
            </div>
            :
            this.props.children
        }
      </div>
    );
  }
}