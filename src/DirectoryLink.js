import {Link} from "react-router-dom";
import folder from "./images/folder.png";
import * as PropTypes from "prop-types";
import React from "react";

export function DirectoryLink(props) {
  const linkStyle = props.dim ? css.linkDim : null;
  const folderStyle = props.dim ? css.folderImgDim : css.folderImg;
  return <Link to={props.to} style={linkStyle}>
    <img alt='folder' style={folderStyle} src={folder}/>{props.children}
  </Link>;
}

const css = {
  folderImg: {
    verticalAlign: 'bottom',
  },
  folderImgDim: {
    verticalAlign: 'bottom',
    filter: 'grayscale() brightness(0.75)',
  },
  linkDim: {
    color: 'var(--neutral2)',
  },
};

DirectoryLink.propTypes = {
  to: PropTypes.string.isRequired,
  dim: PropTypes.bool,
};
