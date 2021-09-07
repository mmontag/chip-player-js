import React from 'react';
const { FORMATS } = require('../config');

const formatList = FORMATS.filter(f => f !== 'miniusf').map(f => `.${f}`);
const splitPoint = Math.floor(formatList.length / 2) - 1;
const formatsLine1 = `Formats: ${formatList.slice(0, splitPoint).join(' ')}`;
const formatsLine2 = formatList.slice(splitPoint).join(' ');

export default class DropMessage extends React.PureComponent {
  render() {
    return <div hidden={!this.props.dropzoneProps.isDragActive} className="message-box-outer">
      <div hidden={!this.props.dropzoneProps.isDragActive} className="message-box drop-message">
        <div className="message-box-inner">
          Drop files to play!<br/>
          {formatsLine1}<br/>
          {formatsLine2}
        </div>
      </div>
    </div>;
  }
}
