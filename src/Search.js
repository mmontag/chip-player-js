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
    const urlParams = {
      q: val ? val : undefined,
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
      this.doSearch(this.props.initialQuery);
    }
    this.setState({
      totalSongs: data.numRecords,
    });
  }

  handleSearchResults(payload) {
    this.setState({
      searching: true,
      resultsCount: payload.count,
      results: payload.results.map(result => result.file).sort(),
    });
  }

  showEmptyState() {
    this.setState({searching: false, results: {}})
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
                              defaultValue={this.state.totalSongs ? this.props.initialQuery : null}
                              onChange={this.onChange}/></label>
        {
          this.state.searching ?
            <span>
              <span>{this.state.resultsCount} result{this.state.resultsCount !== 1 && 's'}</span>
              <div className='Search-results'>
                {this.state.results.map((result, i) => {
                  const href = CATALOG_PREFIX + result;
                  return (
                    <div key={i}><a onClick={this.props.onResultClick(href)} href={href}>{result}</a></div>
                  )
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