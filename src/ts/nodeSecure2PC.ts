import type { IO } from "./types";

/**
 * Runs a secure two-party computation (2PC) using a specified circuit.
 *
 * @param party - The party initiating the computation ('alice' or 'bob').
 * @param circuit - The circuit to run (in this case, a 32-bit addition circuit).
 * @param input - The input to the circuit, represented as a 32-bit binary array.
 * @param io - Input/output channels for communication between the two parties.
 * @returns A promise resolving with the output of the circuit (a 32-bit binary array).
 */
export default async function nodeSecure2PC(
  party: 'alice' | 'bob',
  circuit: string,
  input: Uint8Array,
  io: IO,
): Promise<Uint8Array> {
  const req = typeof require === 'function' ? require : undefined;

  if (!req) {
    throw new Error('Not running in Node.js');
  }

  const jslibPath = '../../../build/jslib.js';

  delete req.cache[req.resolve(jslibPath)];
  
  const Module: {
    emp?: {
      circuit?: string;
      input?: Uint8Array;
      io?: IO;
      handleOutput?: (value: Uint8Array) => void;
    };
    _run(party: number): void;
    onRuntimeInitialized: () => void;
  } = req('../../../build/jslib.js');

  await new Promise<void>((resolve) => {
    Module.onRuntimeInitialized = resolve;
  });

  if (Module.emp) {
    throw new Error('Can only run one secure2PC at a time');
  }

  const emp: { 
    circuit?: string; 
    input?: Uint8Array; 
    io?: IO; 
    handleOutput?: (value: Uint8Array) => void 
  } = {};
  
  Module.emp = emp;

  emp.circuit = circuit;
  emp.input = input;
  emp.io = io;

  const result = new Promise<Uint8Array>((resolve, reject) => {
    try {
      emp.handleOutput = resolve;
      // TODO: emp.handleError

      Module._run(partyToIndex(party));
    } catch (error) {
      reject(error);
    }
  });

  try {
    return await result;
  } finally {
    Module.emp = undefined;
  }
}

/**
 * Maps a party ('alice' or 'bob') to an index number.
 *
 * @param party - The party ('alice' or 'bob').
 * @returns 1 for 'alice', 2 for 'bob'.
 * @throws Will throw an error if the party is invalid.
 */
function partyToIndex(party: 'alice' | 'bob'): number {
  if (party === 'alice') {
    return 1;
  }
  
  if (party === 'bob') {
    return 2;
  }
  
  throw new Error(`Invalid party ${party} (must be 'alice' or 'bob')`);
}
