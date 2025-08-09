import { WebSocketServer } from "ws";
import type http from "http";
import url from "node:url";

import { type SignalingMessage } from "./src/utils/signalingClient";
import type { WebSocket as WsWebSocket } from "ws";
import type { Http2SecureServer } from "http2";

type ClientInfo = {
  id: string;
  ws: WsWebSocket;
  address: string;
  properties: {
    username: string;
    role: string;
    timeJoined: number;
  };
};

export default function WebsocketServer(
  httpServer: http.Server | Http2SecureServer
) {
  const wss = new WebSocketServer({
    // port: 8081,
    noServer: true,
  });

  const clients = new Map<string, ClientInfo>();

  // broadcast message witout sendingn to myself
  function broadcast(data: SignalingMessage) {
    clients.forEach((client) => {
      if (
        client.ws?.readyState === WebSocket.OPEN &&
        client.id !== data.content.id
      ) {
        client.ws.send(JSON.stringify(data));
      }
    });
  }

  httpServer.on("upgrade", (request, socket, head) => {
    if (request.url?.startsWith("/ws")) {
      wss.handleUpgrade(request, socket, head, (ws) => {
        wss.emit("connection", ws, request);
      });
    }
  });

  wss.on("connection", (ws, req) => {
    const params = new URL(req.url || "", `http://${req.headers.host}`)
      .searchParams;

    const id = params.get("id"); // random id
    const username = params.get("username");
    const role = params.get("role");

    if (!id || !username || !role) {
      return ws.terminate();
    }

    const address = req.socket.remoteAddress || "";
    const timeJoined = Date.now();

    const client = {
      id,
      ws,
      address,
      properties: { username, role, timeJoined },
    };

    const clientEnterMsg: SignalingMessage = {
      signalingType: "ClientEnter",
      content: {
        id: client.id,
        address: client.address,
        properties: client.properties,
      },
    };

    broadcast(clientEnterMsg);

    clients.set(id, client);

    console.log(
      `client ${id} ${username} ${address} join, totalClient: ${clients.size}`
    );

    const clientEnteredMsg: SignalingMessage = {
      signalingType: "ClientEntered",
      content: {
        id,
        address,
        properties: {
          username,
          role,
          timeJoined,
        },
      },
    };
    ws.send(JSON.stringify(clientEnteredMsg));

    const clientsMsg: SignalingMessage = {
      signalingType: "Clients",
      content: [...clients.values()]
        .filter((c) => c.id !== id)
        .map((c) => ({
          id: c.id,
          address: c.address,
          properties: c.properties,
        })),
    };
    ws.send(JSON.stringify(clientsMsg));

    ws.on("message", (evt) => {
      const msg = JSON.parse(evt.toString()) as SignalingMessage;

      switch (msg.signalingType) {
        case "Offer": {
          const targetSocket = clients.get(msg.target!);
          const offerMsg: SignalingMessage = {
            signalingType: "Offer",
            sender: client.id,
            senderName: client.properties.username,
            target: msg.target,
            content: msg.content,
          };

          console.log(username, " Sending Offer to", msg.target);
          targetSocket?.ws.send(JSON.stringify(offerMsg));
          break;
        }

        case "Answer": {
          const targetSocket = clients.get(msg.target!);

          const answerMsg: SignalingMessage = {
            signalingType: "Answer",
            sender: client.id,
            senderName: client.properties.username,
            target: msg.target,
            content: msg.content,
          };

          console.log(username, " Sending Answer to", msg.target);
          targetSocket?.ws.send(JSON.stringify(answerMsg));
          break;
        }

        case "Ice": {
          const targetSocket = clients.get(msg.target!);
          const iceMsg: SignalingMessage = {
            signalingType: "Ice",
            sender: client.id,
            target: msg.target,
            content: msg.content,
          };
          console.log(username, " Sending Ice to", msg.target);
          targetSocket?.ws.send(JSON.stringify(iceMsg));

          break;
        }

        default: {
          return;
        }
      }
    });

    ws.on("close", () => {
      // notifiy other clients this ws is exit
      broadcast({
        signalingType: "ClientExit",
        content: {
          id,
          address: client.address,
          properties: { username, role, timeJoined },
        },
      });

      clients.delete(id);

      console.log(`Client ${id} disconnected from custom WebSocket`);
      console.log("Current clients after close:", clients.size);
    });
  });

  return wss;
}
