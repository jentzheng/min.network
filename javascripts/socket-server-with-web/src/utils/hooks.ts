import { createContext, useContext } from "react";

export const ConnectionContext = createContext<{
  pc: RTCPeerConnection | null;
  pcReady: boolean;
}>({
  pc: null,
  pcReady: false,
});

export function useConnection() {
  const context = useContext(ConnectionContext);
  return context;
}
