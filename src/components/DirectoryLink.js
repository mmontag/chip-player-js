import { Link, useHistory } from "react-router-dom";
import folder from "../images/folder.png";
import * as PropTypes from "prop-types";
import React, { memo } from "react";
import queryString from 'querystring';

function getSearch() {
  const urlParams = queryString.parse(window.location.search.substring(1));
  delete urlParams.q;
  return queryString.stringify(urlParams);
}

export default memo(DirectoryLink);

function DirectoryLink(props) {
  const history = useHistory();
  const linkClassName = props.dim ? 'DirectoryLink-dim' : null;
  const folderClassName = props.dim ? 'inline-icon dim-icon' : 'inline-icon';
  // Double encode % because react-router will decode this into history.
  // See https://github.com/ReactTraining/history/issues/505
  // The fix https://github.com/ReactTraining/history/pull/656
  // ...is not released in react-router-dom 5.2.0 which uses history 4.10
  const to = props.to.replace('%25', '%2525');
  const search = props.search || getSearch();

  let toObj = { pathname: to, search: search, state: { prevPathname: window.location.pathname } };
  let onClick = null;

  if (props.isBackLink) {
    onClick = function goBack(e) {
      e.preventDefault();
      history.goBack();
    }
  }

  return (
    <Link to={toObj} className={linkClassName} onClick={onClick} tabIndex="-1">
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
