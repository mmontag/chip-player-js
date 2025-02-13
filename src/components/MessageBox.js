import React, { memo, useCallback, useEffect, useState } from 'react';

export default memo(MessageBox);
function MessageBox(props) {
  const {
    infoTexts = [],
    showInfo,
    toggleInfo
  } = props;

  const [currTextIdx, setCurrTextIdx] = useState(0);
  const infoText = (infoTexts[currTextIdx] || '').trim();
  const numLines = Math.min(40, infoText.split('\n').length + 6);

  useEffect(() => {
    setCurrTextIdx(0);
  }, [infoTexts]);

  const prevInfo = useCallback(() => {
    setCurrTextIdx(Math.max(0, currTextIdx - 1));
  }, [currTextIdx]);

  const nextInfo = useCallback(() => {
    setCurrTextIdx(Math.min(infoTexts.length - 1, currTextIdx + 1));
  }, [infoTexts, currTextIdx]);

  return (<div hidden={!showInfo} className="message-box-outer">
    <div hidden={!showInfo} className="message-box"
         style={{ height: showInfo ? `calc(${numLines} * var(--charH))` : 0 }}>
      <div className="message-box-inner info-text">
        {infoText}
      </div>
      <div className="message-box-footer">
        <button className="box-button message-box-button" onClick={toggleInfo}>Close</button>
        {infoTexts.length > 1 && (
          <span>
            <button disabled={currTextIdx === 0}
                    className="box-button message-box-button"
                    onClick={prevInfo}><span className="inline-icon icon-back"/></button>
              {' '}
              <button disabled={currTextIdx === infoTexts.length - 1}
                      className="box-button message-box-button"
                      onClick={nextInfo}><span className="inline-icon icon-forward"/></button>
          </span>
        )}
      </div>
    </div>
  </div>);
}
