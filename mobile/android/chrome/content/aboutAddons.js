/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Ci = Components.interfaces, Cc = Components.classes, Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm")
Cu.import("resource://gre/modules/AddonManager.jsm");

let gStringBundle = Services.strings.createBundle("chrome://browser/locale/aboutAddons.properties");

let gChromeWin = null;
function getChromeWin() {
  if (!gChromeWin) {
    gChromeWin = window
                .QueryInterface(Ci.nsIInterfaceRequestor)
                .getInterface(Ci.nsIWebNavigation)
                .QueryInterface(Ci.nsIDocShellTreeItem)
                .rootTreeItem
                .QueryInterface(Ci.nsIInterfaceRequestor)
                .getInterface(Ci.nsIDOMWindow)
                .QueryInterface(Ci.nsIDOMChromeWindow);
  }
  return gChromeWin;
}

function init() {
  window.addEventListener("popstate", onPopState, false);
  Services.obs.addObserver(Addons, "browser-search-engine-modified", false);

  AddonManager.addInstallListener(Addons);
  Addons.getAddons();
  showList();
}

function uninit() {
  Services.obs.removeObserver(Addons, "browser-search-engine-modified");
  AddonManager.removeInstallListener(Addons);
}

function openLink(aElement) {
  try {
    let formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].getService(Ci.nsIURLFormatter);
    let url = formatter.formatURLPref(aElement.getAttribute("pref"));
    let BrowserApp = getChromeWin().BrowserApp;
    BrowserApp.addTab(url, { selected: true, parentId: BrowserApp.selectedTab.id });
  } catch (ex) {}
}

function onPopState(aEvent) {
  // Called when back/forward is used to change the state of the page
  if (aEvent.state) {
    // Show the detail page for an addon
    Addons.showDetails(Addons._getElementForAddon(aEvent.state.id));
  } else {
    // Clear any previous detail addon
    let detailItem = document.querySelector("#addons-details > .addon-item");
    detailItem.addon = null;

    showList();
  }
}

function showList() {
  // Hide the detail page and show the list
  let details = document.querySelector("#addons-details");
  details.style.display = "none";
  let list = document.querySelector("#addons-list");
  list.style.display = "block";
}

var Addons = {
  _createItem: function _createItem(aAddon) {
    let outer = document.createElement("div");
    outer.setAttribute("addonID", aAddon.id);
    outer.className = "addon-item";
    outer.setAttribute("role", "button");
    outer.addEventListener("click", function() {
      this.showDetails(outer);
      history.pushState({ id: aAddon.id }, document.title);
    }.bind(this), true);

    let img = document.createElement("img");
    img.className = "favicon";
    img.setAttribute("src", aAddon.iconURL);
    outer.appendChild(img);

    let inner = document.createElement("div");
    inner.className = "inner";

    let details = document.createElement("div");
    details.className = "details";
    inner.appendChild(details);

    let tagPart = document.createElement("div");
    tagPart.textContent = gStringBundle.GetStringFromName("addonType." + aAddon.type);
    tagPart.className = "tag";
    details.appendChild(tagPart);

    let titlePart = document.createElement("div");
    titlePart.textContent = aAddon.name;
    titlePart.className = "title";
    details.appendChild(titlePart);

    let versionPart = document.createElement("div");
    versionPart.textContent = aAddon.version;
    versionPart.className = "version";
    details.appendChild(versionPart);

    if ("description" in aAddon) {
      let descPart = document.createElement("div");
      descPart.textContent = aAddon.description;
      descPart.className = "description";
      inner.appendChild(descPart);
    }

    outer.appendChild(inner);
    return outer;
  },

  _createItemForAddon: function _createItemForAddon(aAddon) {
    let appManaged = (aAddon.scope == AddonManager.SCOPE_APPLICATION);
    let opType = this._getOpTypeForOperations(aAddon.pendingOperations);
    let updateable = (aAddon.permissions & AddonManager.PERM_CAN_UPGRADE) > 0;
    let uninstallable = (aAddon.permissions & AddonManager.PERM_CAN_UNINSTALL) > 0;

    let blocked = "";
    switch(aAddon.blocklistState) {
      case Ci.nsIBlocklistService.STATE_BLOCKED:
        blocked = "blocked";
        break;
      case Ci.nsIBlocklistService.STATE_SOFTBLOCKED:
        blocked = "softBlocked";
        break;
      case Ci.nsIBlocklistService.STATE_OUTDATED:
        blocked = "outdated";
        break;
    }

    let item = this._createItem(aAddon);
    item.setAttribute("isDisabled", !aAddon.isActive);
    item.setAttribute("opType", opType);
    item.setAttribute("updateable", updateable);
    if (blocked)
      item.setAttribute("blockedStatus", blocked);
    item.setAttribute("optionsURL", aAddon.optionsURL || "");
    item.addon = aAddon;

    return item;
  },

  _getElementForAddon: function(aKey) {
    let list = document.getElementById("addons-list");
    let element = list.querySelector("div[addonID='" + aKey + "']");
    return element;
  },

  getAddons: function getAddons() {
    // Clear all content before filling the addons
    let list = document.getElementById("addons-list");
    list.innerHTML = "";

    let self = this;
    AddonManager.getAddonsByTypes(["extension", "theme", "locale"], function(aAddons) {
      for (let i=0; i<aAddons.length; i++) {
        let item = self._createItemForAddon(aAddons[i]);
        list.appendChild(item);
      }

      // Load the search engines
      let defaults = Services.search.getDefaultEngines({ }).map(function (e) e.name);
      function isDefault(aEngine)
        defaults.indexOf(aEngine.name) != -1

      let defaultDescription = gStringBundle.GetStringFromName("addonsSearchEngine.description");

      let engines = Services.search.getEngines({ });
      for (let e = 0; e < engines.length; e++) {
        let engine = engines[e];
        let addon = {};
        addon.id = engine.name;
        addon.type = "search";
        addon.name = engine.name;
        addon.version = "";
        addon.description = engine.description || defaultDescription;
        addon.iconURL = engine.iconURI ? engine.iconURI.spec : "";
        addon.appDisabled = false;
        addon.scope = isDefault(engine) ? AddonManager.SCOPE_APPLICATION : AddonManager.SCOPE_PROFILE;
        addon.engine = engine;

        let item = self._createItem(addon);
        item.setAttribute("isDisabled", engine.hidden);
        item.setAttribute("updateable", "false");
        item.setAttribute("opType", "");
        item.addon = addon;
        list.appendChild(item);
      }
    });
  },

  _getOpTypeForOperations: function _getOpTypeForOperations(aOperations) {
    if (aOperations & AddonManager.PENDING_UNINSTALL)
      return "needs-uninstall";
    if (aOperations & AddonManager.PENDING_ENABLE)
      return "needs-enable";
    if (aOperations & AddonManager.PENDING_DISABLE)
      return "needs-disable";
    return "";
  },

  showDetails: function showDetails(aListItem) {
    let detailItem = document.querySelector("#addons-details > .addon-item");
    detailItem.setAttribute("isDisabled", aListItem.getAttribute("isDisabled"));
    detailItem.setAttribute("opType", aListItem.getAttribute("opType"));
    detailItem.setAttribute("optionsURL", aListItem.getAttribute("optionsURL"));
    let addon = detailItem.addon = aListItem.addon;

    let favicon = document.querySelector("#addons-details > .addon-item .favicon");
    if (addon.iconURL)
      favicon.setAttribute("src", addon.iconURL);
    else
      favicon.removeAttribute("src");

    document.querySelector("#addons-details > .addon-item .title").textContent = addon.name;
    document.querySelector("#addons-details > .addon-item .version").textContent = addon.version;
    document.querySelector("#addons-details > .addon-item .tag").textContent = gStringBundle.GetStringFromName("addonType." + addon.type);
    document.querySelector("#addons-details > .addon-item .description-full").textContent = addon.description;

    let enableBtn = document.getElementById("uninstall-btn");
    if (addon.appDisabled)
      enableBtn.setAttribute("disabled", "true");
    else
      enableBtn.removeAttribute("disabled");

    let uninstallBtn = document.getElementById("uninstall-btn");
    if (addon.scope == AddonManager.SCOPE_APPLICATION)
      uninstallBtn.setAttribute("disabled", "true");
    else
      uninstallBtn.removeAttribute("disabled");

    let box = document.querySelector("#addons-details > .addon-item .options-box");
    box.innerHTML = "";

    // Retrieve the extensions preferences
    try {
      let optionsURL = aListItem.getAttribute("optionsURL");
      let xhr = new XMLHttpRequest();
      xhr.open("GET", optionsURL, false);
      xhr.send();
      if (xhr.responseXML) {
        let currentNode;
        let nodeIterator = xhr.responseXML.createNodeIterator(xhr.responseXML, NodeFilter.SHOW_TEXT, null, false);
        while (currentNode = nodeIterator.nextNode()) {
          let trimmed = currentNode.nodeValue.replace(/^\s\s*/, "").replace(/\s\s*$/, "");
          if (!trimmed.length)
            currentNode.parentNode.removeChild(currentNode);
        }

        // Only allow <setting> for now
        let prefs = xhr.responseXML.querySelectorAll(":root > setting");
        for (let i = 0; i < prefs.length; i++)
          box.appendChild(prefs.item(i));
/*
        // Send an event so add-ons can prepopulate any non-preference based
        // settings
        let event = document.createEvent("Events");
        event.initEvent("AddonOptionsLoad", true, false);
        this.dispatchEvent(event);

        // Also send a notification to match the behavior of desktop Firefox
        let id = this.id.substring(17); // length of |urn:mozilla:item:|
        Services.obs.notifyObservers(document, "addon-options-displayed", id);
*/
      }
    } catch (e) {
      Cu.reportError(e)
    }

    let list = document.querySelector("#addons-list");
    list.style.display = "none";
    let details = document.querySelector("#addons-details");
    details.style.display = "block";
  },

  enable: function enable() {
    let detailItem = document.querySelector("#addons-details > .addon-item");
    if (!detailItem.addon)
      return;

    let opType;
    let isDisabled;
    if (detailItem.addon.type == "search") {
      isDisabled = false;
      detailItem.addon.engine.hidden = false;
      opType = "needs-enable";
    } else if (detailItem.addon.type == "theme") {
      // We can have only one theme enabled, so disable the current one if any
      let theme = null;
      let list = document.getElementById("addons-list");
      let item = list.firstElementChild;
      while (item) {
        if (item.addon && (item.addon.type == "theme") && (item.addon.isActive)) {
          theme = item;
          break;
        }
        item = item.nextSibling;
      }
      if (theme)
        this.disable(theme);

      detailItem.addon.userDisabled = false;
      isDisabled = false;
    } else {
      detailItem.addon.userDisabled = false;
      isDisabled = false;
      opType = this._getOpTypeForOperations(detailItem.addon.pendingOperations);

      if (detailItem.addon.pendingOperations & AddonManager.PENDING_ENABLE) {
        this.showRestart();
      } else {
        if (detailItem.getAttribute("opType") == "needs-disable")
          this.hideRestart();
      }
    }

    detailItem.setAttribute("opType", opType);
    detailItem.setAttribute("isDisabled", isDisabled);

    // Sync to the list item
    let listItem = this._getElementForAddon(detailItem.addon.id);
    listItem.setAttribute("isDisabled", detailItem.getAttribute("isDisabled"));
    listItem.setAttribute("opType", detailItem.getAttribute("opType"));
  },

  disable: function disable() {
    let detailItem = document.querySelector("#addons-details > .addon-item");
    if (!detailItem.addon)
      return;

    let opType;
    let isDisabled;
    if (detailItem.addon.type == "search") {
      isDisabled = true;
      detailItem.addon.engine.hidden = true;
      opType = "needs-disable";
    } else if (detailItem.addon.type == "theme") {
      detailItem.addon.userDisabled = true;
      isDisabled = true;
    } else if (detailItem.addon.type == "locale") {
      detailItem.addon.userDisabled = true;
      isDisabled = true;
    } else {
      detailItem.addon.userDisabled = true;
      opType = this._getOpTypeForOperations(detailItem.addon.pendingOperations);
      isDisabled = !detailItem.addon.isActive;

      if (detailItem.addon.pendingOperations & AddonManager.PENDING_DISABLE) {
        this.showRestart();
      } else {
        if (detailItem.getAttribute("opType") == "needs-enable")
          this.hideRestart();
      }
    }

    detailItem.setAttribute("opType", opType);
    detailItem.setAttribute("isDisabled", isDisabled);

    // Sync to the list item
    let listItem = this._getElementForAddon(detailItem.addon.id);
    listItem.setAttribute("isDisabled", detailItem.getAttribute("isDisabled"));
    listItem.setAttribute("opType", detailItem.getAttribute("opType"));
  },

  uninstall: function uninstall() {
    let list = document.getElementById("addons-list");
    let detailItem = document.querySelector("#addons-details > .addon-item");
    if (!detailItem.addon)
      return;

    let listItem = this._getElementForAddon(detailItem.addon.id);

    if (detailItem.addon.type == "search") {
      // Make sure the engine isn't hidden before removing it, to make sure it's
      // visible if the user later re-adds it (works around bug 341833)
      detailItem.addon.engine.hidden = false;
      Services.search.removeEngine(detailItem.addon.engine);
      // the search-engine-modified observer will take care of updating the list
      history.back();
    } else {
      detailItem.addon.uninstall();
      let opType = this._getOpTypeForOperations(detailItem.addon.pendingOperations);

      if (detailItem.addon.pendingOperations & AddonManager.PENDING_UNINSTALL) {
        this.showRestart();

        // A disabled addon doesn't need a restart so it has no pending ops and
        // can't be cancelled
        if (!detailItem.addon.isActive && opType == "")
          opType = "needs-uninstall";

        detailItem.setAttribute("opType", opType);
        listItem.setAttribute("opType", opType);
      } else {
        list.removeChild(listItem);
        history.back();
      }
    }
  },

  cancelUninstall: function ev_cancelUninstall() {
    let detailItem = document.querySelector("#addons-details > .addon-item");
    if (!detailItem.addon)
      return;

    detailItem.addon.cancelUninstall();
    this.hideRestart();

    let opType = this._getOpTypeForOperations(detailItem.addon.pendingOperations);
    detailItem.setAttribute("opType", opType);

    let listItem = this._getElementForAddon(detailItem.addon.id);
    listItem.setAttribute("opType", opType);
  },

  showRestart: function showRestart(aMode) {
    // TODO (bug 704406)
  },

  hideRestart: function hideRestart(aMode) {
    // TODO (bug 704406)
  },

  onInstallEnded: function(aInstall, aAddon) {
    let needsRestart = false;
    if (aInstall.existingAddon && (aInstall.existingAddon.pendingOperations & AddonManager.PENDING_UPGRADE))
      needsRestart = true;
    else if (aAddon.pendingOperations & AddonManager.PENDING_INSTALL)
      needsRestart = true;

    let list = document.getElementById("addons-list");
    let element = this._getElementForAddon(aAddon.id);
    if (!element) {
      element = this._createItemForAddon(aAddon);
      list.insertBefore(element, list.firstElementChild);
    }

    if (needsRestart)
      element.setAttribute("opType", "needs-restart");
  },

  observe: function observe(aSubject, aTopic, aData) {
    if (aTopic == "browser-search-engine-modified") {
      switch (aData) {
        case "engine-added":
        case "engine-removed":
        case "engine-changed":
          this.getAddons();
          break;
      }
    }
  },

  onInstallFailed: function(aInstall) {
  },

  onDownloadProgress: function xpidm_onDownloadProgress(aInstall) {
  },

  onDownloadFailed: function(aInstall) {
  },

  onDownloadCancelled: function(aInstall) {
  }
}
