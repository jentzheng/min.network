import React, { useState, useEffect, useRef, useCallback } from "react";
import { useConnection } from "./utils/hooks";
import CamaraComponent from "./components/Camara";

interface DataChannelMessage {
  type: string;
  message?: string;
  userId?: string;
  timestamp?: number;
  data?: Record<string, unknown> | string | number;
}

export const Test: React.FC = () => {
  const { webRTCConnection, connectionState, dataChannel } = useConnection();
  const [messages, setMessages] = useState<DataChannelMessage[]>([]);
  const [inputMessage, setInputMessage] = useState("");
  const messagesEndRef = useRef<HTMLDivElement>(null);
  const inBoundVideoRef = useRef<HTMLVideoElement>(null);

  useEffect(() => {
    if (!dataChannel) return;
    dataChannel.onmessage = (event) => {
      const msg = JSON.parse(event.data) as DataChannelMessage;
      setMessages((prev) => [
        ...prev,
        {
          ...msg,
          type: "remote",
          timestamp: msg.timestamp ?? Date.now(),
        },
      ]);
    };
  }, [dataChannel]);

  useEffect(() => {
    if (
      !webRTCConnection ||
      !webRTCConnection.pc ||
      connectionState?.peerState !== "connected"
    ) {
      return;
    }

    const trans = webRTCConnection.pc.getTransceivers();
    if (trans.length === 0) {
      webRTCConnection.pc.addTransceiver("video", { direction: "sendrecv" });
    }
  }, [webRTCConnection, connectionState]);

  const handleCameraStart = useCallback(
    async (stream: MediaStream) => {
      if (
        !webRTCConnection ||
        !webRTCConnection.pc ||
        connectionState?.peerState !== "connected" ||
        !inBoundVideoRef.current
      ) {
        return;
      }

      const videoTrack = stream.getVideoTracks()[0];
      if (videoTrack) {
        const trans = webRTCConnection.pc.getTransceivers();

        if (trans.length > 0) {
          await trans[0].sender.replaceTrack(videoTrack);
          trans[0].direction = "sendrecv";
          const stream = new MediaStream([trans[0].receiver.track]);
          inBoundVideoRef.current.srcObject = stream;
          inBoundVideoRef.current.play();
        }
      }

      console.log("current trans", webRTCConnection.pc.getTransceivers());
    },
    [webRTCConnection, connectionState, inBoundVideoRef]
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

  // useEffect(() => {
  //   if (
  //     !webRTCConnection ||
  //     !webRTCConnection.pc ||
  //     connectionState?.peerState !== "connected"
  //   ) {
  //     return;
  //   }

  //   const videoReceiver = webRTCConnection.pc.getReceivers();
  //   // .find((recv) => recv.track.kind === "video");

  //   console.log("inbound effect", videoReceiver);

  //   // if (videoReceiver && inBoundVideoRef.current) {
  //   //   const stream = new MediaStream([videoReceiver.track]);
  //   //   inBoundVideoRef.current.srcObject = stream;
  //   //   inBoundVideoRef.current.play();
  //   // }
  // }, [webRTCConnection, connectionState]);

  useEffect(() => {
    messagesEndRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [messages]);

  const handleSend = useCallback(
    (e: React.FormEvent) => {
      e.preventDefault();
      // const dataChannel = dcRef.current;
      // const dataChannel = dcRef.current;
      if (!webRTCConnection || !webRTCConnection.dc) {
        console.warn("DataChannel is not open, cannot send message.");
        return;
      }

      try {
        webRTCConnection.dc.send(
          JSON.stringify({
            from: webRTCConnection.signalingClient.properties.username,
            message: inputMessage,
            nested: {
              type: "nestedObj",
              array: [1, 2, 3, 4],
            },
            array: [{ key1: "val" }, { key2: "val" }],
          })
        );

        setMessages((prev) => [
          ...prev,
          { type: "local", message: inputMessage, timestamp: Date.now() },
        ]);

        setInputMessage("");

        console.log("📤 Message sent:", inputMessage);
      } catch (error) {
        console.error("❌ Failed to send message:", error);
      }
    },
    [inputMessage, webRTCConnection]
  );

  return (
    <div className="p-4">
      <div className="grid grid-cols-2 gap-2">
        <CamaraComponent
          onCameraStart={handleCameraStart}
          onCameraStop={handleCameraStop}
        />
        <div>
          <video
            ref={inBoundVideoRef}
            autoPlay
            playsInline
            muted
            controls
            className="w-full h-full  bg-black rounded"
          />
        </div>
      </div>

      <div className="my-4">
        <h3 className="text-lg font-semibold mb-2">Messages</h3>
        <div className="border rounded-lg p-4 h-50 overflow-y-auto bg-white shadow-inner">
          {messages.length === 0 ? (
            <div className="text-gray-500 text-sm">No messages yet...</div>
          ) : (
            messages.map((msg, idx) => (
              <div
                key={idx}
                className={`mb-2 p-2 rounded ${
                  msg.type === "local"
                    ? "bg-blue-50 text-blue-800 ml-8"
                    : "bg-green-50 text-green-800 mr-8"
                }`}
              >
                <div className="flex justify-between items-start">
                  <span className="text-sm font-medium">
                    {msg.type === "local" ? "You" : "Remote"}
                  </span>
                  {msg.timestamp && (
                    <span className="text-xs text-gray-400">
                      {new Date(msg.timestamp).toLocaleTimeString()}
                    </span>
                  )}
                </div>
                <div className="mt-1">{msg.message}</div>
              </div>
            ))
          )}
          <div ref={messagesEndRef} />
        </div>
      </div>

      <form onSubmit={handleSend} className="flex gap-2">
        <input
          className="flex-1 px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
          type="text"
          value={inputMessage}
          onChange={(e) => setInputMessage(e.target.value)}
        />
        <button
          className={`px-4 py-2 rounded-lg font-medium bg-blue-600 text-white hover:bg-blue-700`}
          type="submit"
        >
          Send
        </button>
      </form>
    </div>
  );
};
