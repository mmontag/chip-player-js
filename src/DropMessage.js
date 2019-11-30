import React from "react";

export default function DropMessage(props) {
  return <div hidden={!props.dropzoneProps.isDragActive} className="message-box-outer">
    <div hidden={!props.dropzoneProps.isDragActive} className="message-box drop-message">
      <div className="message-box-inner">
        Drop files to play!<br/>
        Formats: .ay .it .kss .mid .mod <br/>
        .nsf .nsfe .sgc .spc .s3m .vgm .vgz .xm
      </div>
    </div>
  </div>;
}
