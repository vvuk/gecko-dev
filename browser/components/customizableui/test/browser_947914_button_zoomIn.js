/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function() {
  info("Check zoom in button existence and functionality");

  let initialPageZoom = ZoomManager.zoom;
  is(initialPageZoom, 1, "Initial zoom factor should be 1");

  yield PanelUI.show();

  let zoomInButton = document.getElementById("zoom-in-button");
  ok(zoomInButton, "Zoom in button exists in Panel Menu");

  zoomInButton.click();
  let pageZoomLevel = parseInt(ZoomManager.zoom * 100);
  let zoomResetButton = document.getElementById("zoom-reset-button");
  let expectedZoomLevel = parseInt(zoomResetButton.getAttribute("label"), 10);
  ok(pageZoomLevel > 100 && pageZoomLevel == expectedZoomLevel, "Page zoomed in correctly");

  // close the Panel
  let panelHiddenPromise = promisePanelHidden(window);
  PanelUI.hide();
  yield panelHiddenPromise;

  // reset zoom level
  ZoomManager.zoom = initialPageZoom;
});
