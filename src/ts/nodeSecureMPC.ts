import type { IO } from "./types";

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
export default async function nodeSecureMPC(
  party: number,
  size: number,
  circuit: string,
  input: Uint8Array,
  inputBitsPerParty: number[],
  io: IO,
): Promise<Uint8Array> {
  if (typeof process === 'undefined' || typeof process.versions === 'undefined' || !process.versions.node) {
    throw new Error('Not running in Node.js');
  }

  let module = await ((await import('../../build/jslib.js')).default());

  const emp: {
    circuit?: string;
    input?: Uint8Array;
    inputBitsPerParty?: number[];
    io?: IO;
    handleOutput?: (value: Uint8Array) => void;
    handleError?: (error: Error) => void;
  } = {};

  module.emp = emp;

  emp.circuit = circuit;
  emp.input = input;
  emp.inputBitsPerParty = inputBitsPerParty;
  emp.io = io;

  const result = await new Promise<Uint8Array>((resolve, reject) => {
    try {
      emp.handleOutput = resolve;
      emp.handleError = reject;

      module._run(party, size);
    } catch (error) {
      reject(error);
    }
  });

  return result;
}
