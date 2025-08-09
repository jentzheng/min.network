import { defineConfig, type Plugin, type ViteDevServer } from "vite";
import react from "@vitejs/plugin-react-swc";
import tailwindcss from "@tailwindcss/vite";
import basicSsl from "@vitejs/plugin-basic-ssl";
import createSocketIOServer from "./socketIO";
import websocketServer from "./wsserver";

function WebsocketPlugin(): Plugin {
  return {
    name: "custom-websocket",
    configureServer: (server) => {
      if (!server.httpServer) return;

      websocketServer(server.httpServer);
    },
    configurePreviewServer(server) {
      if (!server.httpServer) return;

      websocketServer(server.httpServer);
    },
  };
}

// https://vite.dev/config/
export default defineConfig({
  assetsInclude: ["**/*.onnx"],
  optimizeDeps: {
    exclude: ["onnxruntime-web"],
  },
  server: {
    host: true,
    allowedHosts: [".trycloudflare.com"],
    // proxy: {
    //   "/ws": {
    //     target: "ws://localhost:8081",
    //     ws: true,
    //     changeOrigin: true,
    //   },
    // },
  },
  plugins: [
    react(),
    tailwindcss(),
    // SocketIOServerPlugin(),
    WebsocketPlugin(),
    process.env.SELF_SIGN_SSL === "true" && basicSsl(),
  ],
});
