import { defineConfig, type Plugin } from "vite";
import react from "@vitejs/plugin-react-swc";
import tailwindcss from "@tailwindcss/vite";
import basicSsl from "@vitejs/plugin-basic-ssl";
import createSocketIOServer from "./socketIO";

function SocketIOServerPlugin(): Plugin {
  return {
    name: "websocket-middleware",
    configureServer: (server) => {
      if (!server.httpServer) {
        return;
      }
      createSocketIOServer(server.httpServer);
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
  },
  plugins: [
    react(),
    tailwindcss(),
    SocketIOServerPlugin(),
    // process.env.SELF_SIGN_SSL === "true" && basicSsl(),
    // basicSsl(),
  ],
});
