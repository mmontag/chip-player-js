/* eslint import/no-webpack-loader-syntax: off */
import React, {PureComponent} from 'react';
import SearchWorker from 'worker-loader!./SearchWorker';
import queryString from 'querystring';

const searchWorker = new SearchWorker();

const CATALOG_PREFIX = 'https://gifx.co/music/';
const MAX_RESULTS = 50;

export default class Search extends PureComponent {
  constructor(props) {
    super(props);

    this.doSearch = this.doSearch.bind(this);
    this.onChange = this.onChange.bind(this);
    this.onSearchInputChange = this.onSearchInputChange.bind(this);

    this.handleSearchResults = this.handleSearchResults.bind(this);
    this.handleStatus = this.handleStatus.bind(this);

    searchWorker.onmessage = (message) => this.handleMessage(message.data);

    this.state = {
      searching: false,
      results: {},
      resultsCount: 0,
      totalSongs: 0,
      query: null,
    }
  }

  componentDidUpdate(prevProps) {
    if (!prevProps.catalog && this.props.catalog) {
      console.log('posting catalog load message to worker...');
      searchWorker.postMessage({
        type: 'load',
        payload: JSON.stringify(this.props.catalog),
      });
    }
  }

  onChange(e) {
    this.onSearchInputChange(e.target.value);
  }

  onSearchInputChange(val) {
    this.setState({ query: val });
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

  render() {
    const placeholder = this.state.totalSongs ?
      `${this.state.totalSongs} tunes` : 'Loading catalog...';
    return (
      <section>
        <label>Search: <input type="text"
                              placeholder={placeholder}
                              spellCheck="false"
                              autoComplete="off"
                              autoCorrect="false"
                              autoCapitalize="none"
                              value={this.state.totalSongs ? this.state.query || '' : ''}
                              // defaultValue={this.state.totalSongs ? this.props.initialQuery : null}
                              onChange={this.onChange}/></label>
        {
          this.state.searching ?
            <span>
              <span>{this.state.resultsCount} result{this.state.resultsCount !== 1 && 's'}</span>
              <div className='Search-results'>
                {this.state.results.map((group, i) => {
                  return (
                    <div key={i}>
                      <h5 className='Search-results-group-heading'>
                        <a href='#' onClick={() => this.onSearchInputChange(group.title.replace(/[^a-zA-Z0-9]+/g, ' '))}>
                          {group.title}
                        </a>
                      </h5>
                      {group.items.map((result, i) => {
                        const href = CATALOG_PREFIX + group.title + result;
                        return (
                          <div className='Search-results-group-item' key={i}>
                            <a onClick={this.props.onResultClick(href)} href={href}>{result}</a>
                          </div>
                        )
                      })}
                    </div>
                  );
                })}
              </div>
            </span>
            :
            null
        }
      </section>
    );
  }
}