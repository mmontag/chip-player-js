import React, { memo, useCallback, useContext, useEffect, useMemo, useState } from 'react';
import axios from 'redaxios';
import FavoriteButton from './FavoriteButton';
import { UserContext } from './UserProvider';
import { API_BASE, CATALOG_PREFIX } from '../config';
import { getWithAuth, pathJoin } from '../util';

const NUM_MY_TOP_MONTH = 10;
const NUM_MY_TOP_ALL_TIME = 50;
const NUM_GLOBAL_TOP_MONTH = 10;
const NUM_GLOBAL_TOP_ALL_TIME = 100;
const NUM_GLOBAL_TOP_FAVORITES = 100;

function getRollingMonthDateRange() {
  const end = new Date();
  const start = new Date(Date.now() - 30 * 24 * 60 * 60 * 1000);

  const startMonth = start.toLocaleDateString('en-US', { month: 'long' });
  const startDay = start.getDate();
  const startYear = start.getFullYear();

  const endMonth = end.toLocaleDateString('en-US', { month: 'long' });
  const endDay = end.getDate();
  const endYear = end.getFullYear();

  if (startYear === endYear) {
    return `${startMonth} ${startDay} – ${endMonth} ${endDay}, ${endYear}`;
  }
  return `${startMonth} ${startDay}, ${startYear} – ${endMonth} ${endDay}, ${endYear}`;
}

function TopCharts(props) {
  const {
    currContext,
    currIdx,
    onSongClick,
    handleShufflePlay,
  } = props;

  const { user, loadingUser } = useContext(UserContext);

  const [userSelectedTab, setUserSelectedTab] = useState(null);

  const activeTab = userSelectedTab || (user || loadingUser ? 'my' : 'global');

  const [globalTopMonth, setGlobalTopMonth] = useState([]);
  const [loadingGlobalMonth, setLoadingGlobalMonth] = useState(true);

  const [globalTopAllTime, setGlobalTopAllTime] = useState([]);
  const [loadingGlobalAllTime, setLoadingGlobalAllTime] = useState(true);

  const [globalTopFavorites, setGlobalTopFavorites] = useState([]);
  const [loadingGlobalFavorites, setLoadingGlobalFavorites] = useState(true);

  const [myTopMonth, setMyTopMonth] = useState([]);
  const [loadingMyMonth, setLoadingMyMonth] = useState(false);

  const [myTopAllTime, setMyTopAllTime] = useState([]);
  const [loadingMyAllTime, setLoadingMyAllTime] = useState(false);

  const monthDateRange = useMemo(() => getRollingMonthDateRange(), []);

  // Fetch Global Charts independently
  useEffect(() => {
    let isCancelled = false;
    setLoadingGlobalMonth(true);
    setLoadingGlobalAllTime(true);
    setLoadingGlobalFavorites(true);

    axios.get(`${API_BASE}/top?scope=global&range=month&limit=${NUM_GLOBAL_TOP_MONTH}`)
      .then(res => {
        if (!isCancelled) {
          setGlobalTopMonth(res.data.items || []);
          setLoadingGlobalMonth(false);
        }
      })
      .catch(e => {
        if (!isCancelled) {
          console.error('Error fetching global top month:', e);
          setLoadingGlobalMonth(false);
        }
      });

    axios.get(`${API_BASE}/top?scope=global&range=all&limit=${NUM_GLOBAL_TOP_ALL_TIME}`)
      .then(res => {
        if (!isCancelled) {
          setGlobalTopAllTime(res.data.items || []);
          setLoadingGlobalAllTime(false);
        }
      })
      .catch(e => {
        if (!isCancelled) {
          console.error('Error fetching global top all-time:', e);
          setLoadingGlobalAllTime(false);
        }
      });

    axios.get(`${API_BASE}/top?metric=favorites&limit=${NUM_GLOBAL_TOP_FAVORITES}`)
      .then(res => {
        if (!isCancelled) {
          setGlobalTopFavorites(res.data.items || []);
          setLoadingGlobalFavorites(false);
        }
      })
      .catch(e => {
        if (!isCancelled) {
          console.error('Error fetching global top favorites:', e);
          setLoadingGlobalFavorites(false);
        }
      });

    return () => {
      isCancelled = true;
    };
  }, []);

  // Fetch User Charts independently when user is signed in
  useEffect(() => {
    let isCancelled = false;

    if (!user) {
      setMyTopMonth([]);
      setMyTopAllTime([]);
      setLoadingMyMonth(false);
      setLoadingMyAllTime(false);
      return;
    }

    setLoadingMyMonth(true);
    setLoadingMyAllTime(true);

    getWithAuth(user, `${API_BASE}/top?scope=user&range=month&limit=${NUM_MY_TOP_MONTH}`)
      .then(res => {
        if (!isCancelled) {
          setMyTopMonth(res?.items || []);
          setLoadingMyMonth(false);
        }
      })
      .catch(e => {
        if (!isCancelled) {
          console.error('Error fetching user top month:', e);
          setLoadingMyMonth(false);
        }
      });

    getWithAuth(user, `${API_BASE}/top?scope=user&range=all&limit=${NUM_MY_TOP_ALL_TIME}`)
      .then(res => {
        if (!isCancelled) {
          setMyTopAllTime(res?.items || []);
          setLoadingMyAllTime(false);
        }
      })
      .catch(e => {
        if (!isCancelled) {
          console.error('Error fetching user top all-time:', e);
          setLoadingMyAllTime(false);
        }
      });

    return () => {
      isCancelled = true;
    };
  }, [user]);

  const sections = useMemo(() => {
    if (user && activeTab === 'my') {
      return [
        {
          key: 'my-top-month',
          title: `My Top ${NUM_MY_TOP_MONTH} - Last 30 Days (${monthDateRange})`,
          items: myTopMonth,
          loading: loadingMyMonth,
          emptyMessage: 'No plays recorded in the last 30 days.',
        },
        {
          key: 'my-top-all-time',
          title: `My Top ${NUM_MY_TOP_ALL_TIME} All-Time`,
          items: myTopAllTime,
          loading: loadingMyAllTime,
          emptyMessage: 'No plays recorded yet.',
        },
      ];
    }

    return [
      {
        key: 'global-top-month',
        title: `Global Top ${NUM_GLOBAL_TOP_MONTH} - Last 30 Days (${monthDateRange})`,
        items: globalTopMonth,
        loading: loadingGlobalMonth,
        emptyMessage: 'No plays recorded in the last 30 days.',
      },
      {
        key: 'global-top-all-time',
        title: `Global Top ${NUM_GLOBAL_TOP_ALL_TIME} All-Time`,
        items: globalTopAllTime,
        loading: loadingGlobalAllTime,
        emptyMessage: 'No plays recorded yet.',
      },
      {
        key: 'global-top-favorites',
        title: `Global Top ${NUM_GLOBAL_TOP_FAVORITES} Most Favorited Tracks`,
        metric: 'favorites',
        items: globalTopFavorites,
        loading: loadingGlobalFavorites,
        emptyMessage: 'No favorites recorded yet.',
      },
    ];
  }, [user, activeTab, myTopMonth, myTopAllTime, globalTopMonth, globalTopAllTime, globalTopFavorites, loadingMyMonth, loadingMyAllTime, loadingGlobalMonth, loadingGlobalAllTime, loadingGlobalFavorites, monthDateRange]);

  // Compute decorated sections and unified context across all visible sections
  const { decoratedSections, topContext } = useMemo(() => {
    let currentIdx = 0;
    const contextPaths = [];

    const decorated = sections.map(section => {
      const items = (section.items || []).map((item, localIndex) => {
        const path = item.path;
        contextPaths.push(path);
        const trackIdx = currentIdx++;
        const rank = localIndex + 1;
        const name = path.split('/').pop();
        const href = item.path ? pathJoin(CATALOG_PREFIX, encodeURIComponent(item.path)) : null;
        const url = item.song_id ? `/?play=${encodeURIComponent(item.song_id)}` : href;

        return {
          ...item,
          idx: trackIdx,
          rank,
          name,
          href,
          url,
        };
      });

      return {
        ...section,
        items,
      };
    });

    return { decoratedSections: decorated, topContext: contextPaths };
  }, [sections]);

  const handlePlayTrack = useCallback((trackIdx) => (e) => {
    e.preventDefault();
    onSongClick(null, topContext, trackIdx)(e);
  }, [onSongClick, topContext]);

  const handleShuffle = useCallback(() => {
    if (topContext.length > 0) {
      handleShufflePlay('top', topContext);
    }
  }, [handleShufflePlay, topContext]);

  return (
    <div className="TopCharts">
      <h3 className="Browse-topRow">
        {user ? (
          <div className="TopCharts-tabs">
            <button
              className={`box-button ${activeTab === 'my' ? 'active' : ''}`}
              onClick={() => setUserSelectedTab('my')}
            >
              My Top Charts
            </button>
            <button
              className={`box-button ${activeTab === 'global' ? 'active' : ''}`}
              onClick={() => setUserSelectedTab('global')}
            >
              Global Top Charts
            </button>
          </div>
        ) : (
          <span>Global Top Charts</span>
        )}

        {topContext.length > 1 && (
          <button
            className="box-button"
            title={`Shuffle all ${topContext.length} top chart tracks`}
            onClick={handleShuffle}
          >
            Shuffle Play
          </button>
        )}
      </h3>

      {decoratedSections.map(section => (
        <div key={section.key} className="TopCharts-section">
          <h4 className="TopCharts-sectionTitle">
            {section.title}
          </h4>

          {section.loading && section.items.length === 0 ? (
            <div>Loading...</div>
          ) : section.items.length === 0 ? (
            <div>
              {section.emptyMessage}
            </div>
          ) : (
            <div className="TopCharts-list">
              {section.items.map((item, index) => {
                const isPlaying = currContext === topContext
                  ? currIdx === item.idx
                  : (currContext && currContext[currIdx] === item.path);

                const classNames = ['BrowseList-row'];
                if (isPlaying) classNames.push('Song-now-playing');
                if (index % 2 === 0) classNames.push('even');
                else classNames.push('odd');

                return (
                  <div
                    key={`${section.key}-${item.song_id || item.path}-${item.idx}`}
                    className={classNames.join(' ')}
                    onDoubleClick={handlePlayTrack(item.idx)}
                  >
                    <div className="BrowseList-colRank">{item.rank}.</div>
                    <div className="BrowseList-colName">
                      <FavoriteButton item={item} />
                      <a
                        onClick={handlePlayTrack(item.idx)}
                        href={item.url}
                        tabIndex="-1"
                      >
                        {item.name}
                      </a>
                    </div>
                    <div
                      className="BrowseList-colPlays"
                      title={section.metric === 'favorites' ? `${item.count} favorites` : `${item.plays} plays`}
                    >
                      {section.metric === 'favorites' ? (
                        `${item.count} ${item.count === 1 ? 'favorite' : 'favorites'}`
                      ) : (
                        `${item.plays} ${item.plays === 1 ? 'play' : 'plays'}`
                      )}
                    </div>
                  </div>
                );
              })}
            </div>
          )}
        </div>
      ))}
    </div>
  );
}

export default memo(TopCharts);
