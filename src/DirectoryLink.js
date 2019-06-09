import {Link} from "react-router-dom";
import folder from "./images/folder.png";
import * as PropTypes from "prop-types";
import React from "react";
import queryString from 'querystring';

export default function DirectoryLink(props) {
  const linkStyle = props.dim ? css.linkDim : null;
  const folderStyle = props.dim ? css.folderImgDim : css.folderImg;
  const urlParams = queryString.parse(window.location.search.substr(1));
  delete urlParams.q;
  const search = queryString.stringify(urlParams);
  return <Link to={{ pathname: props.to, search: search }} style={linkStyle}>
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
