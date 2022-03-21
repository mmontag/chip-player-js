import React from 'react';

function Alert(props) {
  const { handlePlayerError, playerError, showPlayerError } = props;

  return (
    <div hidden={!showPlayerError} className="error-box-outer">
      <div className="error-box">
        <div className="message">
          Error: {playerError}
        </div>
        <button className="box-button message-box-button" onClick={() => handlePlayerError(null)}>Close</button>
      </div>
    </div>
  );
}

export default Alert;
