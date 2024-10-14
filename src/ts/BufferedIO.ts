import { EventEmitter } from "ee-typed";
import BufferQueue from "./BufferQueue";
import { IO } from "./types";

export default class BufferedIO
  extends EventEmitter<{ error(e: Error): void }>
  implements IO
{
  private bq = new BufferQueue();

  constructor(public send: IO['send']) {
    super();
  }

  async recv(len: number): Promise<Uint8Array> {
    return await this.bq.pop(len);
  }

  accept(data: Uint8Array) {
    this.bq.push(data);
  }
}
