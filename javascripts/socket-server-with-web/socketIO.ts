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

  io.on("connection", (socket) => {
    const { username, role, roomId } = socket.handshake.query as {
      username: string;
      role: string;
      roomId: string;
    };

    socket.join(roomId);

    socket.to(roomId).emit("newUser", { from: socket.id, username, role });

    socket.on("requestOffer", ({ to }) => {
      socket.to(to).emit("requestOffer", { from: socket.id, username, role });
    });

    socket.on(
      "signal",
      ({ to, description }: { to: string; description: unknown }) => {
        socket.to(to).emit("signal", { from: socket.id, description });
      }
    );

    socket.on(
      "icecandidate",
      ({ to, candidate }: { to: string; candidate: unknown }) => {
        socket.to(to).emit("icecandidate", { from: socket.id, candidate });
      }
    );

    socket.on("disconnecting", () => {
      socket.rooms.forEach((room) => {
        socket.to(room).emit("userLeft", { from: socket.id, role });
      });
    });
  });

  return io;
}
