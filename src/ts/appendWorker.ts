import type { IO } from "./types";

type Module = {
  emp?: {
    circuit?: string;
    inputBits?: Uint8Array;
    inputBitsPerParty?: number[];
    io?: IO;
    handleOutput?: (value: Uint8Array) => void;
    handleState?: (state: string) => void;
  };
  _run_2pc(party: number, size: number): void;
  _run_mpc(party: number, size: number): void;
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
 * @param inputBits - The input bits for the circuit, represented as one bit per byte.
 * @param inputBitsPerParty - The number of input bits for each party.
 * @param io - Input/output channels for communication between the two parties.
 * @returns A promise resolving with the output bits of the circuit.
 */
async function secureMPC({
  party, size, circuit, inputBits, inputBitsPerParty, io, mode = 'auto',
}: {
  party: number,
  size: number,
  circuit: string,
  inputBits: Uint8Array,
  inputBitsPerParty: number[],
  io: IO,
  mode?: '2pc' | 'mpc' | 'auto',
}): Promise<Uint8Array> {
  const module = await createModule();

  if (running) {
    throw new Error('Can only run one secureMPC at a time');
  }

  running = true;

  const emp: {
    circuit?: string;
    inputBits?: Uint8Array;
    inputBitsPerParty?: number[];
    io?: IO;
    handleOutput?: (value: Uint8Array) => void
    handleError?: (error: Error) => void;
    handleState: (state: string) => void;
  } = {
    handleState: state => postMessage({ type: 'state_update', state }),
  };

  module.emp = emp;

  emp.circuit = circuit;
  emp.inputBits = inputBits;
  emp.inputBitsPerParty = inputBitsPerParty;
  emp.io = io;

  const method = calculateMethod(mode, size, circuit);

  const result = new Promise<Uint8Array>(async (resolve, reject) => {
    try {
      emp.handleOutput = resolve;
      emp.handleError = reject;

      emp.handleState('ping_test');
      await pingTest(party, emp);
      emp.handleState('throughput_test');
      await throughputTest(party, emp);

      emp.handleState('enter_wasm');
      module[method](party, size);
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

function calculateMethod(
  mode: '2pc' | 'mpc' | 'auto',
  size: number,

  // Currently unused, but some 2-party circuits might perform better with
  // _runMPC
  _circuit: string,
) {
  switch (mode) {
    case '2pc':
      return '_run_2pc';
    case 'mpc':
      return '_run_mpc';
    case 'auto':
      return size === 2 ? '_run_2pc' : '_run_mpc';

    default:
      const _never: never = mode;
      throw new Error('Unexpected mode: ' + mode);
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
    postMessage({ type: 'state_update', state: 'begin_worker' });
    const { party, size, circuit, inputBits, inputBitsPerParty, mode } = message;

    // Create a proxy IO object to communicate with the main thread
    const io: IO = {
      send: (toParty, channel, data) => {
        postMessage({ type: 'io_send', toParty, channel, data });
      },
      recv: (fromParty, channel, len) => {
        return new Promise((resolve, reject) => {
          const id = requestId++;
          pendingRequests[id] = { resolve, reject };
          postMessage({ type: 'io_recv', fromParty, channel, len, id });
        });
      },
    };

    try {
      const result = await secureMPC({
        party,
        size,
        circuit,
        inputBits,
        inputBitsPerParty,
        io,
        mode,
      });

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

async function pingTest(party: number, emp: Module['emp']) {
  const io = emp!.io!;
  const start = Date.now();
  let resp: unknown;
  let responseCount = 0;

  if (party === 0) {
    while (Date.now() - start < 1000) {
      io.send(1, 'a', Uint8Array.from([13]));
      resp = await io.recv(1, 'a', 1);
      responseCount++;
    }
    io.send(1, 'a', Uint8Array.from([0]));
  } else if (party === 1) {
    while (true) {
      const data = await io.recv(0, 'a', 1);

      if (data[0] === 0) {
        break;
      }

      io.send(0, 'a', data);
    }
  }
  
  const end = Date.now();

  if (party === 0) {
    let duration = end - start;
    let pingAvg = duration / responseCount;
    const msg = `Ping test: ${pingAvg.toFixed(3)}ms (avg), response: ${resp}`;
    postMessage({ type: 'log', msg });
  }
}

async function throughputTest(party: number, emp: Module['emp']) {
  let throughputStartTime = -Infinity;

  const io = emp!.io!;

  const bufSize = 128 * 1024; // 128kB
  const sendCount = 160; // 20MB total (160 * 128kB)
  const totalMB = bufSize * sendCount / 1024 / 1024;

  if (party === 0) {
    console.log('Starting throughput test');
    throughputStartTime = Date.now();

    for (let i = 0; i < sendCount; i++) {
      const buf = new Uint8Array(bufSize);
      for (let j = 0; j < bufSize; j++) {
        buf[j] = (i + j) % 256;
      }
      io.send(1, 'b', buf);
    }
  }

  if (party === 1) {
    for (let i = 0; i < sendCount; i++) {
      const bufReceived = await io.recv(0, 'b', bufSize);

      // if (!buffersEqual(buf, bufReceived)) {
      //   throw new Error('Buffer mismatch 3');
      // }
    }

    // Send a final message to signal the end of the test
    io.send(0, 'b', Uint8Array.from([123]));
  }

  if (party === 0) {
    const bufReceived = await io.recv(1, 'b', 1);

    if (bufReceived[0] !== 123) {
      throw new Error('Buffer mismatch 4');
    }

    const throughputTime = Date.now() - throughputStartTime;
    const msg = `Throughput time: ${throughputTime}ms, MB/s: ${totalMB / (throughputTime / 1000)}`;

    postMessage({ type: 'log', msg });
  }
}
