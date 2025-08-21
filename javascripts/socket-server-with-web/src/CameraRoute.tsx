import { useCallback } from "react";
import { useConnection } from "./utils/hooks";
import CamaraComponent from "./components/Camara";

export const CameraRoute = () => {
  const { webRTCConnection, connectionState } = useConnection();

  const handleCameraStart = useCallback(
    async (stream: MediaStream) => {
      if (
        !webRTCConnection ||
        !webRTCConnection.pc ||
        connectionState?.peerState !== "connected"
      ) {
        return;
      }

      const videoTrack = stream.getVideoTracks()[0];
      if (videoTrack) {
        const trans = webRTCConnection.pc.getTransceivers();

        if (trans.length > 0) {
          await trans[0].sender.replaceTrack(videoTrack);
          trans[0].direction = "sendrecv";
        }
      }

      console.log("current trans", webRTCConnection.pc.getTransceivers());
    },
    [webRTCConnection, connectionState]
  );

  const handleCameraStop = useCallback(
    async (stream: MediaStream) => {
      if (!webRTCConnection?.pc) return;

      console.log("Camera stopped, stream:", stream);
      const sender = webRTCConnection.pc
        .getSenders()
        .find((s) => s.track && s.track.kind === "video");
      if (sender) {
        await sender.replaceTrack(null);
      }
      stream.getTracks().forEach((track) => track.stop());
    },
    [webRTCConnection]
  );

  return (
    <CamaraComponent
      onCameraStart={handleCameraStart}
      onCameraStop={handleCameraStop}
    />
  );
};
