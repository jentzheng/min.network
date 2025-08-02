import React, { useState, useEffect, useRef, useCallback } from "react";
import { useConnection } from "./utils/hooks";
import CamaraComponent from "./components/Camara";

interface WebSocketMessage {
  type: string;
  message?: string;
  userId?: string;
  timestamp?: number;
  data?: Record<string, unknown> | string | number;
}

export const WebSocketTest: React.FC = () => {
  const { pc, pcReady } = useConnection();
  const dcRef = useRef<RTCDataChannel | null>(null);
  const [messages, setMessages] = useState<WebSocketMessage[]>([]);
  const [inputMessage, setInputMessage] = useState("");
  const messagesEndRef = useRef<HTMLDivElement>(null);
  const inBoundVideoRef = useRef<HTMLVideoElement>(null);
  const outBoundVideoRef = useRef<HTMLVideoElement>(null);
  // const [dcStatus] = useState<string>("Not connected");

  // webcam members

  const resolutions = [
    // {
    //   label: "FHD (1920x1080)",
    //   width: { ideal: 1920, max: 1920 },
    //   height: { ideal: 1080, max: 1080 },
    // },
    // {
    //   label: "HD (1280x720)",
    //   width: { ideal: 1280, max: 1280 },
    //   height: { ideal: 720, max: 720 },
    // },
    {
      label: "SD (640x480)",
      width: { ideal: 640, max: 640 },
      height: { ideal: 480, max: 480 },
    },
    {
      label: "Low (320x240)",
      width: { ideal: 320, max: 320 },
      height: { ideal: 240, max: 240 },
    },
  ];

  const [selectedResolution, setSelectedResolution] = useState(resolutions[0]);
  const [videoDevices, setVideoDevices] = useState<MediaDeviceInfo[]>([]);
  const [selectedDeviceId, setSelectedDeviceId] = useState<string | null>(null);
  const [stream, setStream] = useState<MediaStream | null>(null);
  const [facingMode, setFacingMode] = useState<"user" | "environment">("user");

  useEffect(() => {
    if (!pc || !pcReady) {
      return;
    }

    pc.ondatachannel = (evt) => {
      console.log("📡 DataChannel received:", evt.channel);
      dcRef.current = evt.channel;
      setupDataChannelHandlers(evt.channel);
    };

    pc.ontrack = (event) => {
      if (event.track.kind === "video" && inBoundVideoRef.current) {
        if (event.streams && event.streams.length > 0) {
          inBoundVideoRef.current.srcObject = event.streams[0];
          inBoundVideoRef.current.play();
        }
      }
    };

    const setupDataChannelHandlers = (dc: RTCDataChannel) => {
      console.log("📡 Setting up DataChannel listeners");

      const handleOpen = () => {
        console.log("✅ DataChannel opened");
      };

      const handleClose = () => {
        console.log("❌ DataChannel closed");
        dc.close();
      };

      const handleError = (error: Event) => {
        console.error("❌ DataChannel error:", error);
      };

      const handleMessage = (event: MessageEvent) => {
        console.log("💬 DataChannel message received:", event.data);
        setMessages((prev) => [
          ...prev,
          { type: "remote", message: event.data, timestamp: Date.now() },
        ]);
      };

      dc.addEventListener("open", handleOpen);
      dc.addEventListener("close", handleClose);
      dc.addEventListener("error", handleError);
      dc.addEventListener("message", handleMessage);
    };

    return () => {
      pc.onconnectionstatechange = null;
      pc.ondatachannel = null;
      pc.ontrack = null;
    };
  }, [pc, pcReady]);

  useEffect(() => {
    // grab input devices
    (async () => {
      await navigator.mediaDevices.getUserMedia({
        video: true,
      });

      navigator.mediaDevices.enumerateDevices().then((devices) => {
        const videoInputs = devices.filter((d) => d.kind === "videoinput");
        setVideoDevices(videoInputs);
        if (videoInputs.length > 0) {
          setSelectedDeviceId(videoInputs[0].deviceId);
        }
      });
    })();
  }, []);

  const handleStartWebcam = useCallback(async () => {
    const video = outBoundVideoRef.current;
    if (!selectedDeviceId || !video || !pc) return;

    const { width, height } = selectedResolution;

    const mediaStream = await navigator.mediaDevices.getUserMedia({
      video: {
        deviceId: { exact: selectedDeviceId },
        width,
        height,
        facingMode,
      },
    });

    // display locally
    video.srcObject = mediaStream;
    setStream(mediaStream);

    const videoTrack = mediaStream.getVideoTracks()[0];

    // replace video track
    if (pc && videoTrack) {
      const videoTrack = mediaStream.getVideoTracks()[0];

      // 查找已存在的视频 transceiver
      const transceivers = pc.getTransceivers();
      const videoTransceiver = transceivers.find(
        (t) => t.sender.track && t.sender.track.kind === "video"
      );

      if (videoTransceiver) {
        // ✅ 替换为新 track
        await videoTransceiver.sender.replaceTrack(videoTrack);
      } else {
        // 如果没有，直接 addTrack
        pc.addTrack(videoTrack, mediaStream);
      }
    }
  }, [selectedDeviceId, pc, selectedResolution, facingMode]);

  const handleStopWebcam = useCallback(async () => {
    if (stream) {
      stream.getTracks().forEach((track) => {
        track.stop();
      });

      setStream(null);
    }

    if (outBoundVideoRef.current) {
      outBoundVideoRef.current.srcObject = null;
    }

    if (pc) {
      const transceivers = pc.getTransceivers();
      const videoTransceiver = transceivers[0];
      if (videoTransceiver) {
        await videoTransceiver.sender.replaceTrack(null);
      }
    }
  }, [stream, pc]);

  useEffect(() => {
    if (!selectedDeviceId) {
      return;
    }

    handleStartWebcam();
  }, [selectedDeviceId, facingMode, handleStartWebcam]);

  useEffect(() => {
    messagesEndRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [messages]);

  // 发送消息
  const handleSend = (e: React.FormEvent) => {
    e.preventDefault();
    const dc = dcRef?.current;
    if (!dc || dc.readyState !== "open" || !inputMessage.trim()) {
      console.warn("⚠️ DataChannel not ready or message empty");
      return;
    }
    try {
      dc.send(inputMessage);
      setMessages((prev) => [
        ...prev,
        { type: "local", message: inputMessage, timestamp: Date.now() },
      ]);
      setInputMessage("");
      console.log("📤 Message sent:", inputMessage);
    } catch (error) {
      console.error("❌ Failed to send message:", error);
    }
  };

  return (
    <div className="max-w-4xl mx-auto p-6">
      <h2 className="text-2xl font-bold mb-6 text-gray-800">
        DataChannel Test
      </h2>

      <div className="">
        <CamaraComponent />
      </div>

      <div className="flex flex-col p-0">
        <div className="relative w-full">
          <video
            ref={outBoundVideoRef}
            autoPlay
            playsInline
            controls={false}
            className="block w-full max-w-full bg-black  max-h-[60vh] pointer-events-none"
          />
        </div>

        <div className="flex flex-wrap gap-2 p-2">
          <select
            id="videoSource"
            className="select select-bordered flex-1/4"
            onChange={(e) => setSelectedDeviceId(e.target.value)}
            value={selectedDeviceId || ""}
          >
            {videoDevices.map((device) => (
              <option key={device.deviceId} value={device.deviceId}>
                {device.label || device.deviceId}
              </option>
            ))}
          </select>

          <select
            className="select select-bordered flex-1/4"
            onChange={(e) => {
              const res = resolutions[parseInt(e.target.value)];
              setSelectedResolution(res);
            }}
          >
            {resolutions.map((res, index) => (
              <option key={index} value={index}>
                {res.label}
              </option>
            ))}
          </select>

          <button className="btn btn-primary" onClick={handleStartWebcam}>
            Start
          </button>
          <button className="btn btn-warning" onClick={handleStopWebcam}>
            Stop
          </button>
        </div>
      </div>

      <div className="mb-6">
        <h3 className="text-lg font-semibold mb-2">Video Stream</h3>
        <video
          ref={inBoundVideoRef}
          autoPlay
          playsInline
          muted
          controls
          className="w-full max-w-md bg-black rounded"
          style={{ height: 240 }}
          onLoadedMetadata={() => console.log("🎬 Video metadata loaded")}
          onCanPlay={() => console.log("🎬 Video can play")}
          onPlay={() => console.log("🎬 Video started playing")}
          onError={(e) => console.error("❌ Video error:", e)}
        />
      </div>

      <div className="mb-4">
        <h3 className="text-lg font-semibold mb-2">Messages</h3>
        <div className="border rounded-lg p-4 h-48 overflow-y-auto bg-white shadow-inner">
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

      {/* 消息发送表单 */}
      <form onSubmit={handleSend} className="flex gap-2">
        <input
          className="flex-1 px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
          type="text"
          value={inputMessage}
          onChange={(e) => setInputMessage(e.target.value)}
        />
        <button
          className={`px-4 py-2 rounded-lg font-medium ${
            dcRef.current?.readyState && inputMessage.trim()
              ? "bg-blue-600 text-white hover:bg-blue-700"
              : "bg-gray-300 text-gray-500 cursor-not-allowed"
          }`}
          type="submit"
        >
          Send
        </button>
      </form>
    </div>
  );
};
