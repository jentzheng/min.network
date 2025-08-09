import React, { useState, useEffect, useRef, useCallback } from "react";

const resolutions = [
  // {
  //   label: "FHD (1920x1080)",
  //   width: { ideal: 1920, max: 1920 },
  //   height: { ideal: 1080, max: 1080 },
  // },
  {
    label: "HD (1280x720)",
    width: { ideal: 1280, max: 1280 },
    height: { ideal: 720, max: 720 },
  },
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

export default function CamaraComponent({
  onCameraStart,
  onCameraStop,
}: {
  onCameraStart?: (stream: MediaStream) => void;
  onCameraStop?: (stream: MediaStream) => void;
}) {
  const camVideoRef = useRef<HTMLVideoElement>(null);

  const [selectedResolution, setSelectedResolution] = useState(resolutions[0]);
  const [videoDevices, setVideoDevices] = useState<MediaDeviceInfo[]>([]);
  const [selectedDeviceId, setSelectedDeviceId] = useState<string | null>(null);
  const streamRef = useRef<MediaStream | null>(null);

  const [facingMode, setFacingMode] = useState<"user" | "environment">("user");

  const handleStartWebcam = useCallback(async () => {
    const video = camVideoRef.current;
    if (!video || !selectedDeviceId) return;

    if (streamRef.current) {
      streamRef.current.getTracks().forEach((track) => track.stop());
    }

    const { width, height } = selectedResolution;

    const mediaStream = await navigator.mediaDevices.getUserMedia({
      video: {
        deviceId: { exact: selectedDeviceId },
        width,
        height,
        facingMode,
      },
    });

    video.srcObject = mediaStream;
    streamRef.current = mediaStream;

    if (onCameraStart) {
      onCameraStart(mediaStream);
    }
  }, [selectedDeviceId, selectedResolution, facingMode, onCameraStart]);

  const handleStopWebcam = useCallback(async () => {
    const mediaStream = streamRef.current;
    if (!mediaStream) {
      return;
    }

    mediaStream.getTracks().forEach((track) => track.stop());
    streamRef.current = null;

    if (onCameraStop) {
      onCameraStop(mediaStream);
    }
  }, [onCameraStop]);

  // grab input devices
  useEffect(() => {
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

  useEffect(() => {
    if (!selectedDeviceId || !selectedResolution) {
      return;
    }
    handleStartWebcam();
  }, [selectedDeviceId, selectedResolution, handleStartWebcam]);

  return (
    <div className="relative w-full h-full" style={{ minHeight: "400px" }}>
      <div className="relative w-full h-full">
        <video
          ref={camVideoRef}
          autoPlay
          playsInline
          controls={false}
          className="block w-full max-w-full bg-black pointer-events-none"
          style={{ height: "100%", objectFit: "cover" }}
        />
      </div>

      <div
        className="absolute left-0 bottom-0 w-full  p-2"
        style={{ background: "rgba(0,0,0,0.3)" }}
      >
        <div className="flex flex-wrap gap-2 w-full justify-center">
          <div className="dropdown dropdown-top">
            <button tabIndex={0} className="btn btn-secondary">
              {videoDevices.find((d) => d.deviceId === selectedDeviceId)?.label}
            </button>
            <ul tabIndex={0} className="dropdown-content menu p-2 bg-base-100">
              {videoDevices.map((device) => (
                <li key={device.deviceId}>
                  <button
                    className={
                      selectedDeviceId === device.deviceId ? "font-bold" : ""
                    }
                    onClick={() => setSelectedDeviceId(device.deviceId)}
                  >
                    {device.label || device.deviceId}
                  </button>
                </li>
              ))}
            </ul>
          </div>
          <div className="dropdown dropdown-top">
            <label
              tabIndex={0}
              className="btn btn-secondary flex items-center gap-1 cursor-pointer"
              title="resolution"
            >
              <span className="text-xs">{selectedResolution.label}</span>
            </label>
            <ul
              tabIndex={0}
              className="dropdown-content menu p-2 shadow bg-base-100 rounded-box w-52"
            >
              {resolutions.map((res, index) => (
                <li key={index}>
                  <button
                    className={
                      selectedResolution.label === res.label ? "font-bold" : ""
                    }
                    onClick={() => setSelectedResolution(res)}
                  >
                    {res.label}
                  </button>
                </li>
              ))}
            </ul>
          </div>
          <button className="btn btn-primary" onClick={handleStartWebcam}>
            Start
          </button>
          <button className="btn btn-warning" onClick={handleStopWebcam}>
            Stop
          </button>
        </div>
      </div>
    </div>
  );
}
