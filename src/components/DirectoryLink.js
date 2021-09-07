import {Link} from "react-router-dom";
import folder from "../images/folder.png";
import * as PropTypes from "prop-types";
import React from "react";
import queryString from 'querystring';

export default function DirectoryLink(props) {
  const linkClassName = props.dim ? 'DirectoryLink-dim' : null;
  const folderClassName = props.dim ? 'DirectoryLink-folderIconDim' : 'DirectoryLink-folderIcon';
  const urlParams = queryString.parse(window.location.search.substr(1));
  delete urlParams.q;
  // Double encode % because react-router will decode this into history.
  // See https://github.com/ReactTraining/history/issues/505
  // The fix https://github.com/ReactTraining/history/pull/656
  // ...is not released in react-router-dom 5.2.0 which uses history 4.10
  const to = props.to.replace('%25', '%2525');
  const search = queryString.stringify(urlParams);
  return <Link to={{ pathname: to, search: search }} className={linkClassName}>
    <img alt='folder' className={folderClassName} src={folder}/>{props.children}
  </Link>;
}

DirectoryLink.propTypes = {
  to: PropTypes.string.isRequired,
  dim: PropTypes.bool,
};
