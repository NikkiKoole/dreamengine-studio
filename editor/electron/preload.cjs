const { contextBridge, ipcRenderer } = require('electron')

contextBridge.exposeInMainWorld('studio', {
  run:          (code, cfg) => ipcRenderer.invoke('studio:run', code, cfg),
  saveSprites:  (dataUrl) => ipcRenderer.invoke('studio:save-sprites', dataUrl),
})
