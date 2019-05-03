import {Link} from "react-router-dom";
import folder from "./images/folder.png";
import * as PropTypes from "prop-types";
import React from "react";

export function DirectoryLink(props) {
  return <Link to={props.to}>
    <img alt='folder' style={css.folderImg} src={folder}/>{props.children}
  </Link>;
}

const css = {
  folderImg: {
    verticalAlign: 'bottom',
  },
};

DirectoryLink.propTypes = {
  to: PropTypes.string.isRequired,
};