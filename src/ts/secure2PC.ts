import { EventEmitter } from "ee-typed";
import type { IO } from "./types";
import workerSrc from "./workerSrc";
import nodeSecure2PC from "./nodeSecure2PC";

export type Secure2PC = typeof secure2PC;

export default function secure2PC(
  party: 'alice' | 'bob',
  circuit: string,
  input: Uint8Array,
  io: IO,
): Promise<Uint8Array> {
  if (typeof Worker === 'undefined') {
    return nodeSecure2PC(party, circuit, input, io);
  }

  const ev = new EventEmitter<{ cleanup(): void }>();

  const result = new Promise<Uint8Array>((resolve, reject) => {
    const worker = new Worker(workerSrc);
    ev.on('cleanup', () => worker.terminate());

    io.on?.('error', reject);
    ev.on('cleanup', () => io.off?.('error', reject));

    worker.postMessage({
      type: 'start',
      party,
      circuit,
      input,
    });

    worker.onmessage = async (event) => {
      const message = event.data;

      if (message.type === 'io_send') {
        // Forward the send request to the main thread's io.send
        io.send(message.data);
      } else if (message.type === 'io_recv') {
        // Handle the recv request from the worker
        try {
          const data = await io.recv(message.len);
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
