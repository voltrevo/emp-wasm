import { expect } from 'chai';
import { BufferQueue, secureMPC } from "../src/ts"

describe('Secure MPC', () => {
  it('3 + 5 == 8 (2pc)', async function () {
    // Note: This tends to run a bit slower than mpc mode, but that's because
    // of the cold start. Running mpc first is slower than running 2pc first.
    expect(await internalDemo(3, 5, '2pc')).to.deep.equal({ alice: 8, bob: 8 });
  });

  it('3 + 5 == 8 (mpc)', async function () {
    expect(await internalDemo(3, 5, 'mpc')).to.deep.equal({ alice: 8, bob: 8 });
  });

  it('3 + 5 == 8 (auto)', async function () {
    expect(await internalDemo(3, 5, 'auto')).to.deep.equal({ alice: 8, bob: 8 });
  });

  it('3 + 5 == 8 (5 parties)', async function () {
    this.timeout(20_000);
    expect(await internalDemoN(3, 5, 5)).to.deep.equal([8, 8, 8, 8, 8]);
  });
});

class BufferQueueStore {
  bqs = new Map<string, BufferQueue>();

  get(from: number | string, to: number | string, channel: 'a' | 'b') {
    const key = `${from}-${to}-${channel}`;

    if (!this.bqs.has(key)) {
      this.bqs.set(key, new BufferQueue());
    }

    return this.bqs.get(key)!;
  }
}

async function internalDemo(
  aliceInput: number,
  bobInput: number,
  mode: '2pc' | 'mpc' | 'auto' = 'auto',
): Promise<{ alice: number, bob: number }> {
  const bqs = new BufferQueueStore();

  const [aliceBits, bobBits] = await Promise.all([
    secureMPC({
      party: 0,
      size: 2,
      circuit: add32BitCircuit,
      inputBits: numberTo32Bits(aliceInput),
      inputBitsPerParty: [32, 32],
      io: {
        send: (party2, channel, data) => {
          expect(party2).to.equal(1);
          bqs.get('alice', 'bob', channel).push(data);
        },
        recv: async (party2, channel, len) => {
          expect(party2).to.equal(1);
          return bqs.get('bob', 'alice', channel).pop(len);
        },
      },
      mode,
    }),
    secureMPC({
      party: 1,
      size: 2,
      circuit: add32BitCircuit,
      inputBits: numberTo32Bits(bobInput),
      inputBitsPerParty: [32, 32],
      io: {
        send: (party2, channel, data) => {
          expect(party2).to.equal(0);
          bqs.get('bob', 'alice', channel).push(data);
        },
        recv: async (party2, channel, len) => {
          expect(party2).to.equal(0);
          return bqs.get('alice', 'bob', channel).pop(len);
        },
      },
      mode,
    }),
  ]);

  return {
    alice: numberFrom32Bits(aliceBits),
    bob: numberFrom32Bits(bobBits),
  };
}

async function internalDemoN(
  p0Input: number,
  p1Input: number,
  size: number
): Promise<number[]> {
  const bqs = new BufferQueueStore();

  const inputBitsPerParty = new Array(size).fill(0);
  inputBitsPerParty[0] = 32;
  inputBitsPerParty[1] = 32;

  const outputBits = await Promise.all(new Array(size).fill(0).map((_0, party) => secureMPC({
    party,
    size,
    circuit: add32BitCircuit,
    inputBits: (() => {
      if (party === 0) {
        return numberTo32Bits(p0Input);
      }

      if (party === 1) {
        return numberTo32Bits(p1Input);
      }

      return new Uint8Array(0);
    })(),
    inputBitsPerParty,
    io: {
      send: (party2, channel, data) => {
        bqs.get(party, party2, channel).push(data);
      },
      recv: async (party2, channel, len) => {
        return bqs.get(party2, party, channel).pop(len);
      },
    }
  })));

  return outputBits.map(bits => numberFrom32Bits(bits));
}

/**
 * Converts a number into its 32-bit binary representation.
 *
 * @param x - The number to convert.
 * @returns A 32-bit binary representation of the number in the form of a Uint8Array.
 */
function numberTo32Bits(x: number): Uint8Array {
  const result = new Uint8Array(32);

  for (let i = 0; i < 32; i++) {
    result[i] = (x >>> i) & 1;
  }

  return result;
}

/**
 * Converts a 32-bit binary representation back into a number.
 *
 * @param arr - A 32-bit binary array.
 * @returns The number represented by the 32-bit array.
 */
function numberFrom32Bits(arr: Uint8Array): number {
  let result = 0;

  for (let i = 0; i < 32; i++) {
    result |= arr[i] << i;
  }

  return result;
}

const add32BitCircuit = `375 439
32 32   33

2 1 0 32 406 XOR
2 1 5 37 373 AND
2 1 4 36 336 AND
2 1 10 42 340 AND
2 1 14 46 366 AND
2 1 24 56 341 AND
2 1 8 40 342 AND
2 1 1 33 343 AND
2 1 7 39 348 AND
2 1 28 60 349 AND
2 1 19 51 350 AND
2 1 2 34 351 AND
2 1 30 62 364 AND
2 1 13 45 352 AND
2 1 18 50 353 AND
2 1 11 43 355 AND
2 1 3 35 356 AND
2 1 16 48 359 AND
2 1 31 63 357 AND
2 1 27 59 358 AND
2 1 15 47 360 AND
2 1 17 49 361 AND
2 1 9 41 363 AND
2 1 32 0 278 AND
2 1 29 61 362 AND
2 1 6 38 365 AND
2 1 25 57 354 AND
2 1 20 52 367 AND
2 1 22 54 331 AND
2 1 21 53 371 AND
2 1 12 44 372 AND
2 1 23 55 339 AND
2 1 26 58 368 AND
1 1 56 398 INV
1 1 3 314 INV
1 1 40 346 INV
1 1 62 378 INV
1 1 6 389 INV
1 1 28 401 INV
1 1 10 377 INV
1 1 13 391 INV
1 1 27 335 INV
1 1 7 387 INV
1 1 24 399 INV
1 1 54 327 INV
1 1 36 315 INV
1 1 52 332 INV
1 1 50 380 INV
1 1 57 404 INV
1 1 31 323 INV
1 1 55 317 INV
1 1 18 381 INV
1 1 60 400 INV
1 1 5 322 INV
1 1 14 395 INV
1 1 47 402 INV
1 1 8 347 INV
1 1 19 385 INV
1 1 53 374 INV
1 1 29 330 INV
1 1 1 382 INV
1 1 34 344 INV
1 1 20 333 INV
1 1 37 321 INV
1 1 45 390 INV
1 1 11 338 INV
1 1 42 376 INV
1 1 12 370 INV
1 1 38 388 INV
1 1 23 318 INV
1 1 41 392 INV
1 1 61 329 INV
1 1 15 403 INV
1 1 48 396 INV
1 1 26 320 INV
1 1 43 337 INV
1 1 59 334 INV
1 1 9 393 INV
1 1 58 319 INV
1 1 17 326 INV
1 1 44 369 INV
1 1 21 375 INV
1 1 49 325 INV
1 1 16 397 INV
1 1 25 405 INV
1 1 51 384 INV
1 1 4 316 INV
1 1 2 345 INV
1 1 39 386 INV
1 1 46 394 INV
1 1 35 313 INV
1 1 22 328 INV
1 1 63 324 INV
1 1 33 383 INV
1 1 30 379 INV
2 1 313 314 282 AND
2 1 315 316 283 AND
2 1 317 318 284 AND
2 1 319 320 299 AND
2 1 321 322 285 AND
2 1 323 324 286 AND
2 1 325 326 288 AND
2 1 327 328 289 AND
2 1 329 330 290 AND
1 1 331 130 INV
2 1 332 333 287 AND
2 1 334 335 292 AND
1 1 336 256 INV
2 1 337 338 293 AND
1 1 339 123 INV
1 1 340 214 INV
1 1 341 116 INV
1 1 342 228 INV
1 1 343 276 INV
2 1 344 345 310 AND
2 1 346 347 300 AND
1 1 348 235 INV
1 1 349 88 INV
1 1 350 151 INV
1 1 351 270 INV
1 1 352 193 INV
1 1 353 158 INV
1 1 354 109 INV
1 1 355 207 INV
1 1 356 263 INV
1 1 357 66 INV
1 1 358 95 INV
1 1 359 172 INV
1 1 360 179 INV
1 1 361 165 INV
1 1 362 81 INV
1 1 363 221 INV
1 1 364 74 INV
1 1 365 242 INV
1 1 366 186 INV
1 1 367 144 INV
1 1 368 102 INV
2 1 369 370 301 AND
1 1 371 137 INV
1 1 372 200 INV
1 1 373 249 INV
2 1 374 375 298 AND
2 1 376 377 296 AND
2 1 378 379 291 AND
2 1 380 381 297 AND
2 1 382 383 306 AND
2 1 384 385 294 AND
2 1 386 387 295 AND
2 1 388 389 302 AND
2 1 390 391 303 AND
2 1 392 393 304 AND
2 1 394 395 305 AND
2 1 396 397 307 AND
2 1 398 399 308 AND
2 1 400 401 309 AND
2 1 402 403 311 AND
2 1 404 405 312 AND
1 1 282 266 INV
1 1 283 259 INV
1 1 284 126 INV
1 1 285 252 INV
1 1 286 69 INV
1 1 287 147 INV
1 1 288 168 INV
1 1 289 133 INV
1 1 290 84 INV
1 1 291 77 INV
1 1 292 98 INV
1 1 293 210 INV
1 1 294 154 INV
1 1 295 238 INV
1 1 296 217 INV
1 1 297 161 INV
1 1 298 140 INV
1 1 299 105 INV
1 1 300 231 INV
1 1 301 203 INV
1 1 302 245 INV
1 1 303 196 INV
1 1 304 224 INV
1 1 305 189 INV
1 1 306 281 INV
1 1 307 175 INV
1 1 308 119 INV
1 1 309 91 INV
1 1 310 273 INV
1 1 311 182 INV
1 1 312 112 INV
2 1 281 276 277 AND
2 1 69 66 279 AND
2 1 281 278 280 AND
2 1 277 278 407 XOR
1 1 279 71 INV
1 1 280 275 INV
2 1 275 276 274 AND
1 1 274 271 INV
2 1 2 271 268 XOR
2 1 271 273 272 AND
2 1 34 268 408 XOR
1 1 272 269 INV
2 1 269 270 267 AND
1 1 267 264 INV
2 1 3 264 261 XOR
2 1 264 266 265 AND
2 1 35 261 409 XOR
1 1 265 262 INV
2 1 262 263 260 AND
1 1 260 257 INV
2 1 4 257 253 XOR
2 1 257 259 258 AND
2 1 36 253 410 XOR
1 1 258 255 INV
2 1 255 256 254 AND
1 1 254 250 INV
2 1 5 250 247 XOR
2 1 250 252 251 AND
2 1 37 247 411 XOR
1 1 251 248 INV
2 1 248 249 246 AND
1 1 246 243 INV
2 1 6 243 239 XOR
2 1 243 245 244 AND
2 1 38 239 412 XOR
1 1 244 241 INV
2 1 241 242 240 AND
1 1 240 236 INV
2 1 7 236 233 XOR
2 1 236 238 237 AND
2 1 39 233 413 XOR
1 1 237 234 INV
2 1 234 235 232 AND
1 1 232 229 INV
2 1 8 229 226 XOR
2 1 229 231 230 AND
2 1 40 226 414 XOR
1 1 230 227 INV
2 1 227 228 225 AND
1 1 225 222 INV
2 1 9 222 219 XOR
2 1 222 224 223 AND
2 1 41 219 415 XOR
1 1 223 220 INV
2 1 220 221 218 AND
1 1 218 215 INV
2 1 10 215 212 XOR
2 1 215 217 216 AND
2 1 42 212 416 XOR
1 1 216 213 INV
2 1 213 214 211 AND
1 1 211 208 INV
2 1 11 208 205 XOR
2 1 208 210 209 AND
2 1 43 205 417 XOR
1 1 209 206 INV
2 1 206 207 204 AND
1 1 204 201 INV
2 1 12 201 198 XOR
2 1 201 203 202 AND
2 1 44 198 418 XOR
1 1 202 199 INV
2 1 199 200 197 AND
1 1 197 195 INV
2 1 13 195 190 XOR
2 1 195 196 194 AND
2 1 45 190 419 XOR
1 1 194 192 INV
2 1 192 193 191 AND
1 1 191 187 INV
2 1 14 187 183 XOR
2 1 187 189 188 AND
2 1 46 183 420 XOR
1 1 188 185 INV
2 1 185 186 184 AND
1 1 184 180 INV
2 1 15 180 177 XOR
2 1 180 182 181 AND
2 1 47 177 421 XOR
1 1 181 178 INV
2 1 178 179 176 AND
1 1 176 173 INV
2 1 48 173 170 XOR
2 1 173 175 174 AND
2 1 16 170 422 XOR
1 1 174 171 INV
2 1 171 172 169 AND
1 1 169 166 INV
2 1 17 166 163 XOR
2 1 166 168 167 AND
2 1 49 163 423 XOR
1 1 167 164 INV
2 1 164 165 162 AND
1 1 162 159 INV
2 1 18 159 156 XOR
2 1 159 161 160 AND
2 1 50 156 424 XOR
1 1 160 157 INV
2 1 157 158 155 AND
1 1 155 152 INV
2 1 19 152 149 XOR
2 1 152 154 153 AND
2 1 51 149 425 XOR
1 1 153 150 INV
2 1 150 151 148 AND
1 1 148 145 INV
2 1 20 145 141 XOR
2 1 145 147 146 AND
2 1 52 141 426 XOR
1 1 146 143 INV
2 1 143 144 142 AND
1 1 142 138 INV
2 1 53 138 135 XOR
2 1 138 140 139 AND
2 1 21 135 427 XOR
1 1 139 136 INV
2 1 136 137 134 AND
1 1 134 132 INV
2 1 22 132 127 XOR
2 1 132 133 131 AND
2 1 54 127 428 XOR
1 1 131 129 INV
2 1 129 130 128 AND
1 1 128 124 INV
2 1 23 124 121 XOR
2 1 124 126 125 AND
2 1 55 121 429 XOR
1 1 125 122 INV
2 1 122 123 120 AND
1 1 120 117 INV
2 1 24 117 114 XOR
2 1 117 119 118 AND
2 1 56 114 430 XOR
1 1 118 115 INV
2 1 115 116 113 AND
1 1 113 110 INV
2 1 25 110 107 XOR
2 1 110 112 111 AND
2 1 57 107 431 XOR
1 1 111 108 INV
2 1 108 109 106 AND
1 1 106 103 INV
2 1 26 103 100 XOR
2 1 103 105 104 AND
2 1 58 100 432 XOR
1 1 104 101 INV
2 1 101 102 99 AND
1 1 99 96 INV
2 1 59 96 93 XOR
2 1 96 98 97 AND
2 1 27 93 433 XOR
1 1 97 94 INV
2 1 94 95 92 AND
1 1 92 89 INV
2 1 28 89 86 XOR
2 1 89 91 90 AND
2 1 60 86 434 XOR
1 1 90 87 INV
2 1 87 88 85 AND
1 1 85 83 INV
2 1 61 83 79 XOR
2 1 83 84 82 AND
2 1 29 79 435 XOR
1 1 82 80 INV
2 1 80 81 78 AND
1 1 78 76 INV
2 1 30 76 72 XOR
2 1 76 77 75 AND
2 1 62 72 436 XOR
1 1 75 73 INV
2 1 73 74 70 AND
2 1 70 71 437 XOR
1 1 70 68 INV
2 1 68 69 67 AND
1 1 67 65 INV
2 1 65 66 64 AND
1 1 64 438 INV
`;
