import http from "node:http";
import type { Http2SecureServer } from "node:http2";
import { Server } from "socket.io";

export default function createSocketIOServer(
  httpServer: http.Server | Http2SecureServer
) {
  const io = new Server(httpServer, {
    cors: {
      origin: "*",
    },
  });

  io.on("connection", async (socket) => {
    const { username, role } = socket.handshake.query as {
      username: string;
      role: string;
    };

    const clients = await (
      await io.fetchSockets()
    )
      .filter((s) => s.id !== socket.id)
      .map((s) => {
        return {
          id: s.id,
          address: s.handshake.address,
          properties: {
            username: s.handshake.query["username"],
            role: s.handshake.query["role"],
            timeJoined: s.handshake.issued,
          },
        };
      });

    io.to(socket.id).emit("Clients", clients);

    io.to(socket.id).emit("ClientEntered", {
      id: socket.id,
      address: socket.handshake.address,
      properties: { username, role, timeJoined: socket.handshake.issued },
    });

    socket.broadcast.emit("ClientEnter", {
      id: socket.id,
      address: socket.handshake.address,
      properties: { username, role, timeJoined: socket.handshake.issued },
    });

    socket.on(
      "signal",
      ({
        to,
        description,
      }: {
        to: string;
        description: RTCSessionDescriptionInit;
      }) => {
        // const mLines = description.sdp
        //   ?.split("\n")
        //   .filter((line) => line.startsWith("m="));

        // console.log(
        //   `${username} sending ${description.type} to ${to} m-lines:`,
        //   mLines
        // );
        socket.to(to).emit("signal", { from: socket.id, description });
      }
    );

    socket.on(
      "candidate",
      ({ to, candidate }: { to: string; candidate: RTCIceCandidate }) => {
        socket.to(to).emit("candidate", { from: socket.id, candidate });
      }
    );

    socket.on("disconnecting", () => {
      socket.broadcast.emit("ClientExit", {
        id: socket.id,
        address: socket.handshake.address,
        properties: { username, role },
      });
    });
  });

  return io;
}
