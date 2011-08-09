// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
var ContextCommands = {
  copy: function cc_copy() {
    let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);
    clipboard.copyString(ContextHelper.popupState.string);

    let target = ContextHelper.popupState.target;
    if (target)
      target.focus();
  },

#ifdef ANDROID
  selectInput: function cc_selectInput() {
    let imePicker = Cc["@mozilla.org/imepicker;1"].getService(Ci.nsIIMEPicker);
    imePicker.show();
  },
#endif

  paste: function cc_paste() {
    let target = ContextHelper.popupState.target;
    if (target.localName == "browser") {
      let x = ContextHelper.popupState.x;
      let y = ContextHelper.popupState.y;
      let json = {x: x, y: y, command: "paste" };
      messageManager.sendAsyncMessage("Browser:ContextCommand", json);
    } else {
      target.editor.paste(Ci.nsIClipboard.kGlobalClipboard);
      target.focus();
    }
  },

  selectAll: function cc_selectAll() {
    let target = ContextHelper.popupState.target;
    if (target.localName == "browser") {
      let x = ContextHelper.popupState.x;
      let y = ContextHelper.popupState.y;
      let json = {x: x, y: y, command: "select-all" };
      messageManager.sendAsyncMessage("Browser:ContextCommand", json);
    } else {
      target.editor.selectAll();
      target.focus();
    }
  },

  openInNewTab: function cc_openInNewTab() {
    Browser.addTab(ContextHelper.popupState.linkURL, false, Browser.selectedTab);
  },

  saveLink: function cc_saveLink() {
    let browser = ContextHelper.popupState.target;
    ContentAreaUtils.saveURL(ContextHelper.popupState.linkURL, null, "SaveLinkTitle", false, true, browser.documentURI);
  },

  saveImage: function cc_saveImage() {
    let popupState = ContextHelper.popupState;
    let browser = popupState.target;

    // Bug 638523
    // Using directly SaveImageURL fails here since checking the cache for a
    // remote page seems to not work (could it be nsICacheSession prohibition)?
    ContentAreaUtils.internalSave(popupState.mediaURL, null, null,
                                  popupState.contentDisposition,
                                  popupState.contentType, false, "SaveImageTitle",
                                  null, browser.documentURI, true, null);
  },

  copyLink: function cc_copyLink() {
    let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);
    clipboard.copyString(ContextHelper.popupState.linkURL);
  },

  shareLink: function cc_shareLink() {
    let state = ContextHelper.popupState;
    SharingUI.show(state.linkURL, state.linkTitle);
  },

  shareMedia: function cc_shareMedia() {
    SharingUI.show(ContextHelper.popupState.mediaURL, null);
  },

  bookmarkLink: function cc_bookmarkLink() {
    let state = ContextHelper.popupState;
    let bookmarks = PlacesUtils.bookmarks;
    try {
      bookmarks.insertBookmark(BookmarkList.panel.mobileRoot,
                               Util.makeURI(state.linkURL),
                               bookmarks.DEFAULT_INDEX,
                               state.linkTitle || state.linkURL);
    } catch (e) {
      return;
    }

    let message = Strings.browser.GetStringFromName("alertLinkBookmarked");
    let toaster = Cc["@mozilla.org/toaster-alerts-service;1"].getService(Ci.nsIAlertsService);
    toaster.showAlertNotification(null, message, "", false, "", null);
  },

  sendCommand: function cc_playVideo(aCommand) {
    let browser = ContextHelper.popupState.target;
    browser.messageManager.sendAsyncMessage("Browser:ContextCommand", { command: aCommand });
  },

  editBookmark: function cc_editBookmark() {
    let target = ContextHelper.popupState.target;
    target.startEditing();
  },

  removeBookmark: function cc_removeBookmark() {
    let target = ContextHelper.popupState.target;
    target.remove();
  },

  shortcutBookmark: function cc_shortcutBookmark() {
    const kIconSize = 64;

    let target = ContextHelper.popupState.target;
    let canvas = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    canvas.setAttribute("style", "display: none");

    let self = this;
    let image = new Image();
    image.onload = function() {
      canvas.width = canvas.height = kIconSize; // clears the canvas
      let ctx = canvas.getContext("2d");
      ctx.drawImage(image, 0, 0, kIconSize, kIconSize);
      let icon = canvas.toDataURL("image/png", "");
      canvas = null;
      try {
        let shell = Cc["@mozilla.org/browser/shell-service;1"].createInstance(Ci.nsIShellService);
        shell.createShortcut(target.getAttribute("title"), target.getAttribute("uri"), icon, "bookmark");
      } catch(e) {
        Cu.reportError(e);
      }
    }

    image.src = target.getAttribute("src");
  }
};
