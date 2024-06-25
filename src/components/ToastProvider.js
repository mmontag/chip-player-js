import React, { createContext, useCallback, useEffect, useRef, useState } from 'react';

const TOAST_DURATION_MS = 6000;
const TOAST_QUEUE_LIMIT = 5;

const ToastContext = createContext();

const ToastProvider = ({ children }) => {
  const [queue, setQueue] = useState([]);

  const timerRef = useRef();

  const enqueueToast = (message, level) => {
    while (queue.length >= TOAST_QUEUE_LIMIT) {
      queue.splice(0, 1);
    }
    setQueue([...queue, { message, level }]);
  };

  const clearCurrentToast = useCallback(() => {
    setQueue(queue.slice(1));
  }, [queue]);

  useEffect(() => {
    if (queue.length > 0) {
      clearTimeout(timerRef.current);
      timerRef.current = setTimeout(() => clearCurrentToast(), TOAST_DURATION_MS);
    }

    return () => clearTimeout(timerRef.current);
  }, [queue, clearCurrentToast]);

  return (
    <ToastContext.Provider
      value={{
        enqueueToast,
        clearCurrentToast,
        currentToast: queue[0],
        toastCount: queue.length
      }}
    >
      {children}
    </ToastContext.Provider>
  );
};

export { ToastContext, ToastProvider };
