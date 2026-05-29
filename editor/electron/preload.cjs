const { contextBridge, ipcRenderer, webUtils } = require('electron')

contextBridge.exposeInMainWorld('studio', {
  run:          (code, cfg) => ipcRenderer.invoke('studio:run', code, cfg),
  saveSprites:  (dataUrl) => ipcRenderer.invoke('studio:save-sprites', dataUrl),
  saveMap:      (bytes)   => ipcRenderer.invoke('studio:save-map', bytes),
  onLog:        (cb) => ipcRenderer.on('cart:log',  (_, s)    => cb(s)),
  onExit:       (cb) => ipcRenderer.on('cart:exit', (_, info) => cb(info)),
  saveCart:     (payload)  => ipcRenderer.invoke('cart:save', payload),
  loadCart:     ()         => ipcRenderer.invoke('cart:load'),
  loadCartBuffer: (bytes)   => ipcRenderer.invoke('cart:load-buffer', bytes),
  loadCartFile:   (filePath) => ipcRenderer.invoke('cart:load-file', filePath),
  getFilePath:  (file)     => webUtils.getPathForFile(file),
  buildWeb:     (code, cfg) => ipcRenderer.invoke('studio:build-web', code, cfg),
})
