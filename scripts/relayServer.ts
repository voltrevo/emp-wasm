import * as WebSocket from 'ws';
import * as http from 'http';
import * as url from 'url';

const server = http.createServer();
const wss = new WebSocket.WebSocketServer({ server });

interface ConnectionPair {
  clients: [WebSocket | null, WebSocket | null];
  buffer: Buffer[];
  bufferSize: number;
  timeout: NodeJS.Timeout;
}

const pairs: Map<string, ConnectionPair> = new Map();

wss.on('connection', (ws: WebSocket, req: http.IncomingMessage) => {
  // Extract pairing ID from the URL path
  const parsedUrl = url.parse(req.url || '', true);
  const pathname = parsedUrl.pathname || '';
  const pairingId = pathname.slice(1); // Remove leading '/'

  if (!pairingId) {
    ws.close(1008, 'Pairing ID required');
    return;
  }

  let pair = pairs.get(pairingId);

  if (!pair) {
    // First client connects
    pair = {
      clients: [ws, null],
      buffer: [],
      bufferSize: 0,
      timeout: setTimeout(() => {
        // Close connection if second client doesn't connect within 1 minute
        ws.close(1008, 'Second client did not connect in time');
        pairs.delete(pairingId);
      }, 60000),
    };
    pairs.set(pairingId, pair);
  } else if (pair.clients[1] === null) {
    // Second client connects
    pair.clients[1] = ws;

    // Send buffered messages to the second client
    for (const message of pair.buffer) {
      ws.send(message);
    }
    pair.buffer = [];
    pair.bufferSize = 0;
    clearTimeout(pair.timeout);
  } else {
    // Reject connection if more than two clients try to use the same pairing ID
    ws.close(1008, 'Pairing ID already has two clients');
    return;
  }

  ws.onmessage = (ev: MessageEvent) => {
    const data = ev.data;

    const otherClient = pair.clients[0] === ws ? pair.clients[1] : pair.clients[0];
    if (otherClient && otherClient.readyState === WebSocket.WebSocket.OPEN) {
      // Relay message directly to the other client
      otherClient.send(data);
    } else {
      // Buffer the message
      let messageBuffer: Buffer;

      if (typeof data === 'string') {
        messageBuffer = Buffer.from(data);
      } else if (data instanceof Buffer) {
        messageBuffer = data;
      } else if (data instanceof ArrayBuffer) {
        messageBuffer = Buffer.from(data);
      } else if (Array.isArray(data)) {
        messageBuffer = Buffer.concat(data);
      } else {
        // Unsupported data type
        return;
      }

      pair.bufferSize += messageBuffer.length;
      if (pair.bufferSize > 1 * 1024 * 1024) {
        // Terminate connection if buffer exceeds 1MB
        ws.close(1009, 'Message buffer exceeded 1MB');
        pairs.delete(pairingId);
        if (pair.clients[0] && pair.clients[0] !== ws) pair.clients[0].close(1009, 'Message buffer exceeded 1MB');
        if (pair.clients[1] && pair.clients[1] !== ws) pair.clients[1].close(1009, 'Message buffer exceeded 1MB');
      } else {
        pair.buffer.push(messageBuffer);
      }
    }
  };

  ws.onclose = () => {
    // Clean up when clients disconnect
    if (pair.clients[0] === ws) {
      pair.clients[0] = null;
    } else if (pair.clients[1] === ws) {
      pair.clients[1] = null;
    }
    if (!pair.clients[0] && !pair.clients[1]) {
      pairs.delete(pairingId);
      clearTimeout(pair.timeout);
    }
  };

  ws.onerror = (err) => {
    console.error(`WebSocket error: ${err}`);
  };
});

server.listen(8175, () => {
  console.log('running: ws://localhost:8175/pairing-id');
});
