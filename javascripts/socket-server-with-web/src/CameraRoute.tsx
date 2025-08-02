import { useRef, useState, useEffect, useCallback } from "react";
import { useConnection } from "./utils/hooks";

export const CameraRoute = () => {
  const { pc } = useConnection();
  const videoRef = useRef<HTMLVideoElement | null>(null);
  const [videoDevices, setVideoDevices] = useState<MediaDeviceInfo[]>([]);
  const [selectedDeviceId, setSelectedDeviceId] = useState<string | null>(null);
  const [stream, setStream] = useState<MediaStream | null>(null);
  const [facingMode, setFacingMode] = useState<"user" | "environment">("user");

  const resolutions = [
    {
      label: "FHD (1920x1080)",
      width: { ideal: 1920, max: 1920 },
      height: { ideal: 1080, max: 1080 },
    },
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

  const [selectedResolution, setSelectedResolution] = useState(resolutions[0]);

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
    const video = videoRef.current;
    if (!selectedDeviceId || !video || !pc) return;

    const { width, height } = selectedResolution;

    const mediaStream = await navigator.mediaDevices.getUserMedia({
      video: {
        deviceId: selectedDeviceId,
        width,
        height,
        facingMode,
      },
    });

    // display locally
    video.srcObject = mediaStream;
    setStream(mediaStream);

    // replace video track
    if (pc) {
      const videoTrack = mediaStream.getVideoTracks()[0];
      if (videoTrack) {
        // console.log("🎬 Adding video track to PeerConnection");
        const transceivers = pc.getTransceivers();
        console.log(transceivers, "----");
        await transceivers[0].sender.replaceTrack(videoTrack);
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

    if (videoRef.current) {
      videoRef.current.srcObject = null;
    }

    if (pc) {
      const transceivers = pc.getTransceivers();
      const videoTransceiver = transceivers[0];

      if (videoTransceiver) {
        await videoTransceiver.sender.replaceTrack(null);
      }
    }
  }, [stream, pc]);

  const handleFullscreen = () => {
    const video = videoRef.current;
    if (!video) return;

    if ("webkitEnterFullscreen" in video) {
      // @ts-ignore
      video.webkitEnterFullscreen(); // for iOS Safari
    } else if (video.requestFullscreen) {
      video.requestFullscreen(); // Android, desktop
    }
  };

  useEffect(() => {
    if (!selectedDeviceId) {
      return;
    }

    handleStartWebcam();
  }, [selectedDeviceId, facingMode, handleStartWebcam]);

  // useEffect(() => {
  // if (!stream || !pc) return;

  // const videoTrack = stream.getVideoTracks()[0];
  // if (!videoTrack) return;

  // const sender = pc.getSenders().find((s) => s.track?.kind === "video");

  // if (sender) {
  //   sender.replaceTrack(videoTrack);
  // } else {
  //   pc.addTrack(videoTrack, stream);
  // }

  // return () => {
  //   if (stream) {
  //     stream.getTracks().forEach((track) => {
  //       track.stop();
  //     });
  //   }
  // };
  // }, [stream, pc]);

  return (
    <div className="flex flex-col p-0">
      <div className="relative w-full">
        <video
          ref={videoRef}
          autoPlay
          playsInline
          controls={false}
          className="block w-full max-w-full bg-black  max-h-[60vh] pointer-events-none"
        />
        <button
          className="btn absolute top-2 right-2 "
          onClick={handleFullscreen}
        >
          Fullscreen
        </button>
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
  );
};
