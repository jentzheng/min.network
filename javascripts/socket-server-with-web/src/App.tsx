import { useState, useEffect, useCallback, useRef } from "react";
import { NavLink, Outlet, useSearchParams } from "react-router";
import { ConnectionContext } from "./utils/hooks";
import { io, type Socket } from "socket.io-client";

function App() {
  const [searchParams, setSearchParams] = useSearchParams();
  const [formState, setFormState] = useState({
    username: searchParams.get("username") || "Chrome#1",
    roomId: searchParams.get("roomId") || "JitterBridge",
  });

  const socketIORef = useRef<Socket>(
    io({
      autoConnect: false,
      reconnection: true,
      query: {
        username: formState.username,
        roomId: formState.roomId,
      },
    })
  );

  // When managing an RTCPeerConnection instance in a React component,
  // useRef is generally the preferred hook over useState.
  const remoteSocketId = useRef<string | null>(null);
  const pcRef = useRef<RTCPeerConnection | null>(null);
  const dcRef = useRef<RTCDataChannel | null>(null);
  const videoTrackRef = useRef<MediaStreamTrack | null>(null);

  const [connectionState, setConnectionState] = useState({
    socket: false,
    rtcConnectionState: "unknown",
  });

  const [pcReady, setPcReady] = useState(false);

  useEffect(() => {
    const socket = socketIORef.current;
    if (socket && !socket.connected) {
      socket.io.opts.query = {
        username: formState.username,
        roomId: formState.roomId,
      };
    }
  }, [formState]);

  const createPeerConnection = useCallback(() => {
    const pc = new RTCPeerConnection();
    pcRef.current = pc;
    setPcReady(true);

    // webrtc callback
    const pcListeners: [
      keyof RTCPeerConnectionEventMap,
      (event: never) => void
    ][] = [
      [
        "negotiationneeded",
        (evt: Event) => {
          console.log("🔄 Negotiation needed", evt);
        },
      ],
      [
        "signalingstatechange",
        async () => {
          console.log("Signaling state change", pc.signalingState);

          switch (pc.signalingState) {
            case "have-remote-offer": {
              if (remoteSocketId.current) {
                const answer = await pc.createAnswer();
                await pc.setLocalDescription(answer);
                socketIORef.current.emit("signal", {
                  to: remoteSocketId.current,
                  description: answer,
                });
              }
              break;
            }
            case "have-local-offer": {
              break;
            }
            case "have-local-pranswer": {
              break;
            }
            case "closed": {
              break;
            }
            default: {
              return;
            }
          }
        },
      ],
      [
        "connectionstatechange",
        () => {
          console.log("connectionstatechange", pc.connectionState);
          if (pc.connectionState === "disconnected") {
            //

            setConnectionState((prev) => ({
              ...prev,
              rtcConnectionState: pc.connectionState,
            }));
          }
        },
      ],
      [
        "icecandidate",
        (evt: RTCPeerConnectionIceEvent) => {
          if (evt.candidate && remoteSocketId.current) {
            socketIORef.current.emit("icecandidate", {
              to: remoteSocketId.current,
              candidate: evt.candidate,
            });
          }
        },
      ],
      [
        "icegatheringstatechange",
        () => {
          console.log("icegatheringstatechange", pc.iceGatheringState);
        },
      ],
      [
        "datachannel",
        (evt: RTCDataChannelEvent) => {
          console.log("---datachannel", evt.channel);
          dcRef.current = evt.channel;
        },
      ],
      [
        "track",
        (evt: RTCTrackEvent) => {
          videoTrackRef.current = evt.track;
        },
      ],
    ];

    pcListeners.forEach(([event, handler]) => {
      pc.addEventListener(event, handler);
    });

    return pc;
  }, []);

  const removePeerConnection = useCallback(() => {
    const pc = pcRef.current;
    if (!pc) return;
    pc.close();
    pcRef.current = null;
    setPcReady(false);
  }, []);

  useEffect(() => {
    const socket = socketIORef.current;
    if (!socket) {
      return;
    }

    const socketListeners: [string, (event: any) => void][] = [
      [
        "connect",
        () => {
          console.log(`Socket.IO Server connected. id: ${socket.id}`);
          setConnectionState((prev) => ({
            ...prev,
            socket: socket.connected,
          }));
          createPeerConnection();
        },
      ],
      [
        "newUser",
        (evt: { from: string; role: string; username: string }) => {
          if (evt.role === "host") {
            socket.emit("requestOffer", { to: evt.from });
            createPeerConnection();
          }
        },
      ],
      [
        "userLeft",
        (evt: { from: string; role: string }) => {
          if (evt.role === "host") {
            removePeerConnection();
          }
        },
      ],
      [
        "signal",
        async (evt) => {
          remoteSocketId.current = evt.from;
          switch (evt.description.type) {
            case "offer": {
              if (pcRef.current) {
                console.log("Offer received");
                await pcRef.current.setRemoteDescription(evt.description);
              }
              break;
            }
            default: {
              return;
            }
          }
        },
      ],
      [
        "icecandidate",
        async (evt: { from: string; candidate: RTCIceCandidate }) => {
          if (evt.candidate && pcRef.current) {
            await pcRef.current.addIceCandidate(evt.candidate);
          }
        },
      ],
      [
        "disconnect",
        () => {
          setConnectionState((prev) => ({
            ...prev,
            socket: socket.connected,
          }));
        },
      ],
    ];

    socketListeners.forEach(([event, handler]) => {
      socket.on(event, handler);
    });

    socket.connect();

    return () => {
      console.log("🧹 Cleaning up...");
      socketListeners.forEach(([event, handler]) => {
        socket.off(event, handler);
      });
      remoteSocketId.current = null;
    };
  }, [createPeerConnection, removePeerConnection]);

  useEffect(() => {
    setSearchParams({
      username: formState.username,
      roomId: formState.roomId,
    });
  }, [formState, setSearchParams]);

  const handleChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const { name, value } = e.target;
    setFormState((prev) => ({ ...prev, [name]: value }));
  };

  const handleConnect = useCallback((e: React.FormEvent) => {
    e.preventDefault();
    console.log("trigger form submit");
    if (socketIORef.current.connected) {
      socketIORef.current.close();
    } else {
      socketIORef.current.connect();
    }
  }, []);

  return (
    <ConnectionContext.Provider value={{ pc: pcRef.current, pcReady }}>
      <div className="drawer lg:drawer-open" data-theme="cupcake">
        {/* Sidebar Toggle Button */}
        <input id="sidebar-toggle" type="checkbox" className="drawer-toggle" />

        {/* Main Layout */}
        <div className="drawer-content flex flex-col max-h-screen">
          {/* Header: hamburger + nav links */}
          <header className="flex items-center gap-4 px-4 py-2  bg-base-100">
            <label
              htmlFor="sidebar-toggle"
              className="btn btn-square btn-ghost lg:hidden"
            >
              <svg
                xmlns="http://www.w3.org/2000/svg"
                className="h-6 w-6"
                fill="none"
                viewBox="0 0 24 24"
                stroke="currentColor"
              >
                <path
                  strokeLinecap="round"
                  strokeLinejoin="round"
                  strokeWidth="2"
                  d="M4 6h16M4 12h16M4 18h16"
                />
              </svg>
            </label>

            <nav
              role="tablist"
              className="tabs tabs-bordered flex-grow overflow-x-auto"
            >
              <NavLink
                role="tab"
                className={({ isActive }) =>
                  isActive ? "tab tab-active" : "tab"
                }
                to="/"
              >
                Test
              </NavLink>
              <NavLink
                role="tab"
                className={({ isActive }) =>
                  isActive ? "tab tab-active" : "tab"
                }
                to="/detection"
              >
                Detection
              </NavLink>
              <NavLink
                role="tab"
                className={({ isActive }) =>
                  isActive ? "tab tab-active" : "tab"
                }
                to="/camera"
              >
                Camera
              </NavLink>
            </nav>
          </header>

          {/* Main Content */}
          <main className="bg-gray-600 flex-1 overflow-auto">
            <Outlet />
          </main>
        </div>

        <aside className="drawer-side">
          <label htmlFor="sidebar-toggle" className="drawer-overlay"></label>
          <div className="menu p-4 w-80 min-h-full bg-base-200 text-base-content">
            <form onSubmit={handleConnect}>
              <div className="grid gap-4">
                <label className="form-control w-full">
                  <div className="label">
                    <span className="label-text">username</span>
                  </div>
                  <input
                    type="text"
                    name="username"
                    className="input input-bordered w-full"
                    disabled={connectionState.socket}
                    value={formState.username}
                    onChange={handleChange}
                  />
                </label>

                <label className="form-control w-full">
                  <div className="label">
                    <span className="label-text">room</span>
                  </div>
                  <input
                    type="text"
                    name="roomId"
                    className="input input-bordered w-full"
                    disabled={connectionState.socket}
                    value={formState.roomId}
                    onChange={handleChange}
                  />
                </label>

                <button type="submit" className="btn btn-primary">
                  {connectionState.socket ? "Disconnect" : "Connect"}
                </button>
              </div>
            </form>

            <ul className="mt-1">
              <li>{`Socket status: ${
                connectionState.socket ? "Connected" : "Disconnected"
              }`}</li>
              {/* <li>PC state: {connectionState.peerState}</li> */}
            </ul>
          </div>
        </aside>
      </div>
    </ConnectionContext.Provider>
  );
}

export default App;
