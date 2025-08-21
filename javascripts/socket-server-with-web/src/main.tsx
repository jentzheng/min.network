import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import { BrowserRouter, Routes, Route } from "react-router";

import App from "./App.tsx";
import { CameraRoute } from "./CameraRoute.tsx";
import { FaceLandmarkRoute } from "./FaceLandmarkRoute.tsx";
import { Test } from "./Test.tsx";
import "./index.css";

const root = document.getElementById("root");

if (root) {
  createRoot(root).render(
    //<StrictMode>
    <BrowserRouter>
      <Routes>
        <Route path="/" element={<App />}>
          <Route index element={<Test />} />
          <Route path="facelandmark" element={<FaceLandmarkRoute />} />
          <Route path="camera" element={<CameraRoute />} />
        </Route>
      </Routes>
    </BrowserRouter>
    //</StrictMode>
  );
}
