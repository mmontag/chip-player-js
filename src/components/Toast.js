import React, { memo, useContext, useEffect, useRef } from 'react';
import { ToastContext } from './ToastProvider';

export const ToastLevels = {
  INFO: {
    label: 'Info',
    className: 'toast-info',
  },
  ERROR: {
    label: 'Error',
    className: 'toast-error',
  },
};

// Naive implementation. For advanced implementation, see
// https://www.developerway.com/posts/implementing-advanced-use-previous-hook
function usePrevious(value) {
  const ref = useRef();
  useEffect(() => {
    ref.current = value;
  });
  return ref.current;
}

const Toast = () => {
  const { clearCurrentToast, currentToast, toastCount } = useContext(ToastContext);
  const previousToast = usePrevious(currentToast);

  const { message, level = ToastLevels.INFO } = currentToast || previousToast || {};
  const className = ['toast-box', level?.className].join(' ');
  const countLabel = toastCount > 1 ? ` (${toastCount})` : '';

  return (
    <div hidden={!currentToast} className="toast-box-outer">
      <div className={className}>
        <div className="message">
          {level?.label}: {message}
        </div>
        <button className="box-button message-box-button" onClick={clearCurrentToast}>Dismiss{countLabel}</button>
      </div>
    </div>
  );
}

export default memo(Toast);
