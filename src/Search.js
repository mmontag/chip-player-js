/* eslint import/no-webpack-loader-syntax: off */
import React, {PureComponent} from 'react';
import SearchWorker from 'worker-loader!./SearchWorker';

const searchWorker = new SearchWorker();

const CATALOG_PREFIX = 'https://gifx.co/music/';
const MAX_RESULTS = 30;

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
    this.setState({
      totalSongs: data.numRecords,
    });
  }

  handleSearchResults(results) {
    this.setState({
      searching: true,
      resultsCount: results.length,
      results: results.map(result => result.file),
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
                              onChange={this.onChange}/></label>
        {
          this.state.searching ?
            this.state.resultsCount === 0 ?
              <span> No results.</span>
              :
              <div className='Search-results'>
                {this.state.results.map((result, i) => {
                  const href = CATALOG_PREFIX + result;
                  return (
                    <div key={i}><a onClick={this.props.onResultClick(href)} href={href}>{result}</a></div>
                  )
                })}
              </div>
            :
            null
        }
      </section>
    );
  }
}