import React, { useCallback, useEffect, useRef, useState } from 'react';
import { ArrowKeyStepper, List, WindowScroller } from 'react-virtualized';
import 'react-virtualized/styles.css';
import { findDOMNode } from 'react-dom';
import { useHistory } from 'react-router-dom';
import { pathJoin } from '../util';


export default VirtualizedList;

function VirtualizedList(props) {
  const {
    scrollContainerRef,
    currContext,
    currIdx,
    onSongClick,
    itemList,
    songContext,
    rowRenderer,
    listRef,
    isSorted,
  } = props;

  const arrowKeyStepperRef = useRef();
  const history = useHistory();
  const [selectedRow, setSelectedRow] = useState(0);
  const updateHistoryRef = useRef();

  // Create a new 'update history' function every time selection changes.
  // TODO: Use a selectedRowRef instead, so that the callback doesn't have to be updated?
  useEffect(() => {
    updateHistoryRef.current = () => {
      if (!scrollContainerRef.current) return;
      const newState = {
        ...history.location.state,
        selectedRow,
        scrollTop: Math.round(scrollContainerRef.current.scrollTop),
      };
      console.log('Updating history %s with new state:', history.location.pathname, newState);
      history.replace(history.location.pathname, newState);
    };
  }, [selectedRow, scrollContainerRef, history]);

  useEffect(() => {
    // history.block runs *before* navigation.
    const unblock = history.block((location, action) => {
      if (!updateHistoryRef.current) return;
      console.log(`History updating to location: ${location.pathname} with action: ${action}`);
      if (action === 'PUSH') {
        updateHistoryRef.current();
      }
    });
    return () => unblock();  // Cleanup on unmount
  }, [history, updateHistoryRef]);

  const onActivate = useCallback((index) => {
    // Song index may differ from item index if there are directories
    const item = itemList[index];
    const songIndex = item.idx ?? index;

    if (item.type === 'directory') {
      return (e) => {
        console.log('Directory clicked', index, e);
        e.preventDefault();
        if (item.isBackLink) {
          history.goBack();
        } else {
          history.push(pathJoin('/browse', item.path), {
            prevPathname: window.location.pathname,
          });
        }
      }
    } else {
      return onSongClick(null, songContext, songIndex);
    }
  }, [history, itemList, songContext, onSongClick]);

  // Callback for ArrowKeyStepper.
  const selectCell = useCallback(({ scrollToRow }) => {
    setSelectedRow(scrollToRow);
  }, [setSelectedRow]);

  // Reset/restore selected row when itemList changes.
  useEffect(() => {
    const scrollTop = history.location.state?.scrollTop || 0;
    const row = history.location.state?.selectedRow || 0;
    setSelectedRow(row);
    // Ensure list is focused after a top-level (Browse, Favorites, etc) navigation.
    if (listRef) findDOMNode(listRef.current).focus({ preventScroll: true });
    // TODO: pass scroll position and selected row from parent, along with itemList.
    // This can't be sync because 'setSelectedRow' is async.
    setTimeout(() => {
      scrollContainerRef.current.scrollTo(0, scrollTop);
    }, 0);
  }, [history, itemList, scrollContainerRef, listRef]);

  return (
    <div onKeyDown={(e) => {
      // TODO: Possibly remove ArrowKeyStepper and just use this instead.
      if (!e.repeat && (e.key === 'Enter' || e.key === 'Return')) {
        const index = listRef.current.props.scrollToIndex;
        listRef.current.scrollToRow(index);
        onActivate(index)(e);
      } else if (e.key === 'Home') {
        setSelectedRow(0);
      } else if (e.key === 'End') {
        setSelectedRow(itemList.length - 1);
      } else if (e.key === 'PageUp') {
        e.preventDefault();
        setSelectedRow(Math.max(0, selectedRow - 10));
      } else if (e.key === 'PageDown') {
        e.preventDefault();
        setSelectedRow(Math.min(itemList.length - 1, selectedRow + 10));
      } else if (e.key.length === 1 && isSorted) {
        e.preventDefault();
        const char = e.key.toLowerCase();
        let left = 0;
        let right = itemList.length - 1;
        let mid;
        while (left <= right) {
          mid = Math.floor((left + right) / 2);
          const name = itemList[mid].name.toLowerCase();
          if (name.startsWith(char)) {
            break;
          } else if (name.toLowerCase() < char) {
            left = mid + 1;
          } else {
            right = mid - 1;
          }
        }
        // Rewind to first instance of char
        while (mid > 0 && itemList[mid - 1].name.toLowerCase()[0] === char) mid--;
        setSelectedRow(mid);
      }
    }}>
      <WindowScroller
        scrollElement={scrollContainerRef.current}
        onScroll={({ scrollTop }) => {
          listRef.current.scrollToPosition(scrollTop);
          // Focus problems https://github.com/bvaughn/react-virtualized/issues/776
          findDOMNode(listRef.current).focus({ preventScroll: true });
        }}
      >
        {({ height, registerChild, onChildScroll, scrollTop }) => (
          <>
            {props.children}
            <div ref={registerChild}>
            <ArrowKeyStepper
              ref={arrowKeyStepperRef}
              columnCount={1}
              rowCount={itemList.length}
              mode="cells"
              isControlled={true}
              scrollToRow={selectedRow}
              onScrollToChange={selectCell}
            >
              {({ onSectionRendered, scrollToRow }) => (
                <List
                  ref={listRef}
                  onRowsRendered={({ startIndex, stopIndex }) => {
                    onSectionRendered({ rowStartIndex: startIndex, rowStopIndex: stopIndex })
                  }}
                  scrollToIndex={scrollToRow}
                  scrollToAlignment="auto"
                  autoHeight
                  height={height || 0}
                  width={500}
                  onScroll={onChildScroll}
                  containerProps= {{ autoFocus: true }}
                  containerStyle={{ width: "100%", maxWidth: "100%" }}
                  style={{ width: "100%" }}
                  rowCount={itemList.length}
                  rowHeight={19}
                  rowRenderer={({ index, key, style }) => {
                    const item = itemList[index];
                    // Song index may differ from item index if there are directories
                    const songIndex = item.idx;
                    const isPlaying = currContext === songContext && currIdx === songIndex;
                    const isSelected = index === scrollToRow;
                    const classNames = ['BrowseList-row'];
                    if (isPlaying) classNames.push('Song-now-playing');
                    if (isSelected) classNames.push('BrowseList-row-selected');

                    // NOTE: Why not use ref.current.focus(), and rely on browser default behaviors?
                    // Focusing <a> element seems to force it to center of viewport.
                    // It is also impossible to play an item scrolled off screen if relying on <a> focus.
                    // For these reasons we will use a fake focus instead of the browser built-in.

                    const onSelect = (e) => {
                      setSelectedRow(index);
                      // Prevent focus on the anchor element
                      e.preventDefault();
                      // Bubble focus up to the containing List component (it should have a tabIndex)
                      let el = e.target.parentElement;
                      while (el?.tabIndex < 0) el = el.parentElement;
                      el?.focus({
                        preventScroll: true
                      });
                    };

                    const onActivateWithIndex = onActivate(index);

                    return <div key={key} style={style}
                                className={classNames.join(' ')}
                                onMouseUp={onSelect}
                                onDoubleClick={onActivateWithIndex}>{
                      rowRenderer({
                        item,
                        onPlay: onActivateWithIndex,
                      })
                    }</div>;
                  }}
                />
              )}
            </ArrowKeyStepper>
            </div>
          </>
        )}
      </WindowScroller>
    </div>
  );
}
