import { EventEmitter } from "ee-typed";
import type { IO } from "./types";
import workerSrc from "./workerSrc.js";
import nodeSecure2PC from "./nodeSecure2PC.js";

export type Secure2PC = typeof secure2PC;

export default function secure2PC(
  party: number,
  size: number,
  circuit: string,
  input: Uint8Array,
  inputBitsStart: number,
  io: IO,
): Promise<Uint8Array> {
  if (typeof Worker === 'undefined') {
    return nodeSecure2PC(party, size, circuit, input, inputBitsStart, io);
  }

  const ev = new EventEmitter<{ cleanup(): void }>();

  const result = new Promise<Uint8Array>((resolve, reject) => {
    const worker = new Worker(workerSrc, { type: 'module' });
    ev.on('cleanup', () => worker.terminate());

    io.on?.('error', reject);
    ev.on('cleanup', () => io.off?.('error', reject));

    worker.postMessage({
      type: 'start',
      party,
      size,
      circuit,
      input,
      inputBitsStart,
    });

    worker.onmessage = async (event) => {
      const message = event.data;

      if (message.type === 'io_send') {
        // Forward the send request to the main thread's io.send
        const { party2, channel, data } = message;
        io.send(party2, channel, data);
      } else if (message.type === 'io_recv') {
        const { party2, channel, len } = message;
        // Handle the recv request from the worker
        try {
          const data = await io.recv(party2, channel, len);
          worker.postMessage({ type: 'io_recv_response', id: message.id, data });
        } catch (error) {
          worker.postMessage({
            type: 'io_recv_error',
            id: message.id,
            error: (error as Error).message,
          });
        }
      } else if (message.type === 'result') {
        // Resolve the promise with the result from the worker
        resolve(message.result);
      } else if (message.type === 'error') {
        // Reject the promise if an error occurred
        reject(new Error(message.error));
      }
    };

    worker.onerror = reject;
  });

  return result.finally(() => ev.emit('cleanup'));
}
