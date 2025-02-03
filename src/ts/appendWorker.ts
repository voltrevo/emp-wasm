import type { IO } from "./types";

type Module = {
  emp?: {
    circuit?: string;
    input?: Uint8Array;
    inputBitsPerParty?: number[];
    io?: IO;
    handleOutput?: (value: Uint8Array) => void;
  };
  _run(party: number, size: number): void;
  onRuntimeInitialized: () => void;
};

let running = false;

declare const createModule: () => Promise<Module>

/**
 * Runs a secure multi-party computation (MPC) using a specified circuit.
 *
 * @param party - The party index joining the computation (0, 1, .. N-1).
 * @param size - The number of parties in the computation.
 * @param circuit - The circuit to run.
 * @param input - The input to the circuit, represented as one bit per byte.
 * @param io - Input/output channels for communication between the two parties.
 * @returns A promise resolving with the output bits of the circuit.
 */
async function secureMPC(
  party: number,
  size: number,
  circuit: string,
  input: Uint8Array,
  inputBitsPerParty: number[],
  io: IO,
): Promise<Uint8Array> {
  const module = await createModule();

  if (running) {
    throw new Error('Can only run one secureMPC at a time');
  }

  running = true;

  const emp: {
    circuit?: string;
    input?: Uint8Array;
    inputBitsPerParty?: number[];
    io?: IO;
    handleOutput?: (value: Uint8Array) => void
    handleError?: (error: Error) => void;
  } = {};

  module.emp = emp;

  emp.circuit = circuit;
  emp.input = input;
  emp.inputBitsPerParty = inputBitsPerParty;
  emp.io = io;

  const result = new Promise<Uint8Array>((resolve, reject) => {
    try {
      emp.handleOutput = resolve;
      emp.handleError = reject;

      module._run(party, size);
    } catch (error) {
      reject(error);
    }
  });

  try {
    return await result;
  } finally {
    running = false;
  }
}

let requestId = 0;

const pendingRequests: {
  [id: number]: {
    resolve: (data: Uint8Array) => void;
    reject: (error: Error) => void;
  };
} = {};

onmessage = async (event) => {
  const message = event.data;

  if (message.type === 'start') {
    const { party, size, circuit, input, inputBitsPerParty } = message;

    // Create a proxy IO object to communicate with the main thread
    const io: IO = {
      send: (party2, channel, data) => {
        postMessage({ type: 'io_send', party2, channel, data });
      },
      recv: (party2, channel, len) => {
        return new Promise((resolve, reject) => {
          const id = requestId++;
          pendingRequests[id] = { resolve, reject };
          postMessage({ type: 'io_recv', party2, channel, len, id });
        });
      },
    };

    try {
      const result = await secureMPC(party, size, circuit, input, inputBitsPerParty, io);
      postMessage({ type: 'result', result });
    } catch (error) {
      postMessage({ type: 'error', error: (error as Error).message });
    }
  } else if (message.type === 'io_recv_response') {
    const { id, data } = message;
    if (pendingRequests[id]) {
      pendingRequests[id].resolve(data);
      delete pendingRequests[id];
    }
  } else if (message.type === 'io_recv_error') {
    const { id, error } = message;
    if (pendingRequests[id]) {
      pendingRequests[id].reject(new Error(error));
      delete pendingRequests[id];
    }
  }
};
