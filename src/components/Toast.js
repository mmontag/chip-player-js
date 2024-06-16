import React from 'react';

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

function Toast(props) {
  const { handleClose, showToast, toast } = props;
  const { message, level = ToastLevels.INFO } = toast;
  const className = ['toast-box', level?.className].join(' ');

  return (
    <div hidden={!showToast} className="toast-box-outer">
      <div className={className}>
        <div className="message">
          {level?.label}: {message}
        </div>
        <button className="box-button message-box-button" onClick={handleClose}>Close</button>
      </div>
    </div>
  );
}

export default Toast;
