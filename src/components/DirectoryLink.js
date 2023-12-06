import { Link, useLocation } from "react-router-dom";
import folder from "../images/folder.png";
import * as PropTypes from "prop-types";
import React, { memo } from "react";
import queryString from 'querystring';

function getSearch(location) {
  const urlParams = queryString.parse(location.search.substring(1));
  delete urlParams.q;
  return queryString.stringify(urlParams);
}

export default memo(DirectoryLink);

function DirectoryLink(props) {
  const location = useLocation();
  const linkClassName = props.dim ? 'DirectoryLink-dim' : null;
  const folderClassName = props.dim ? 'DirectoryLink-folderIconDim' : 'DirectoryLink-folderIcon';
  // Double encode % because react-router will decode this into history.
  // See https://github.com/ReactTraining/history/issues/505
  // The fix https://github.com/ReactTraining/history/pull/656
  // ...is not released in react-router-dom 5.2.0 which uses history 4.10
  const to = props.to.replace('%25', '%2525');
  const search = props.search || getSearch(location);

  let toObj = { pathname: to, search: search, state: { prevPathname: location.pathname } };
  let onClick = null;

  if (props.isBackLink) {
    onClick = function goBack(e) {
      e.preventDefault();
      // history.back();
      props.history.goBack();
    }
  }

  return (
    <Link to={toObj} className={linkClassName} onClick={onClick}>
      <img alt='folder' className={folderClassName} src={folder}/>{props.children}
    </Link>
  );
}

DirectoryLink.propTypes = {
  to: PropTypes.string.isRequired,
  dim: PropTypes.bool,
  search: PropTypes.string,
  isBackLink: PropTypes.bool,
};
