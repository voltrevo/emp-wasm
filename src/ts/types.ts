export type IO = {
  send: (data: Uint8Array) => void;
  recv: (len: number) => Promise<Uint8Array>;
  on?: (event: 'error', listener: (error: Error) => void) => void;
  off?: (event: 'error', listener: (error: Error) => void) => void;
  close?: () => void;
};
