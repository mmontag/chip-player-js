import React from 'react';
const { FORMATS } = require('../config');

const formatList = FORMATS.map(f => `.${f}`);
const formatsLine = `Formats: ${formatList.join(' ')}`;

export default class DropMessage extends React.PureComponent {
  render() {
    return <div hidden={!this.props.dropzoneProps.isDragActive} className="message-box-outer">
      <div hidden={!this.props.dropzoneProps.isDragActive} className="message-box drop-message">
        <div className="message-box-inner">
          Drop files to play!<br/>
          {formatsLine}<br/>
        </div>
      </div>
    </div>;
  }
}
