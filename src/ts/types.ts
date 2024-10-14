export type IO = {
  send: (data: Uint8Array) => void;
  recv: (len: number) => Promise<Uint8Array>;
};
