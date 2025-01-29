export type IO = {
  send: (party2: number, channel: 'a' | 'b', data: Uint8Array) => void;
  recv: (party2: number, channel: 'a' | 'b', len: number) => Promise<Uint8Array>;
  on?: (event: 'error', listener: (error: Error) => void) => void;
  off?: (event: 'error', listener: (error: Error) => void) => void;
  close?: () => void;
};
