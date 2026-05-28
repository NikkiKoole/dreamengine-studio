const { app, BrowserWindow, ipcMain, Menu, session } = require('electron')
const { exec, spawn }                  = require('child_process')
const path                             = require('path')
const fs                               = require('fs')

const RUNTIME_DIR = path.join(__dirname, '../../runtime')
const BUILD_DIR   = path.join(__dirname, '../../build')
const RAYLIB      = '/usr/local/opt/raylib'
const CART_SRC    = path.join(BUILD_DIR, 'cart.c')
const CART_BIN    = path.join(BUILD_DIR, 'cart')

function createWindow() {
  const win = new BrowserWindow({
    width: 1280,
    height: 800,
    titleBarStyle: 'hiddenInset',
    backgroundColor: '#1b1c1f',
    webPreferences: {
      preload: path.join(__dirname, 'preload.cjs'),
      contextIsolation: true,
    },
  })

  win.loadURL('http://localhost:5173')

  win.webContents.on('before-input-event', (event, input) => {
    if (!input.meta && !input.control) return
    switch (input.key.toLowerCase()) {
      case 'c': event.preventDefault(); win.webContents.copy();      break
      case 'v': event.preventDefault(); win.webContents.paste();     break
      case 'x': event.preventDefault(); win.webContents.cut();       break
      case 'a': event.preventDefault(); win.webContents.selectAll(); break
      case 'z': event.preventDefault()
        input.shift ? win.webContents.redo() : win.webContents.undo()
        break
    }
  })
}

app.whenReady().then(() => {
  session.defaultSession.setPermissionRequestHandler((_wc, permission, callback) => {
    callback(permission === 'clipboard-read' || permission === 'clipboard-write' || permission === 'clipboard-sanitized-write')
  })
  session.defaultSession.setPermissionCheckHandler((_wc, permission) => {
    return permission === 'clipboard-read' || permission === 'clipboard-write' || permission === 'clipboard-sanitized-write'
  })
  Menu.setApplicationMenu(Menu.buildFromTemplate([
    {
      label: 'dreamengine',
      submenu: [{ role: 'quit' }]
    },
    {
      label: 'Edit',
      submenu: [
        { role: 'undo' },
        { role: 'redo' },
        { type: 'separator' },
        { role: 'cut' },
        { role: 'copy' },
        { role: 'paste' },
        { role: 'selectAll' },
      ]
    }
  ]))
  createWindow()
})

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit()
})

const FONT_SRC  = path.join(__dirname, '../public/dos_8x8.png')
const FONT_DEST = path.join(BUILD_DIR, 'dos_8x8.png')

// ── sprites handler ───────────────────────────────────────────
ipcMain.handle('studio:save-sprites', (_event, dataUrl) => {
  const base64 = dataUrl.replace(/^data:image\/png;base64,/, '')
  fs.mkdirSync(BUILD_DIR, { recursive: true })
  fs.writeFileSync(path.join(BUILD_DIR, 'sprites.png'), base64, 'base64')
})

// ── run handler ───────────────────────────────────────────────
ipcMain.handle('studio:run', async (_event, code, cfg) => {
  const screenW = cfg?.screenW || 320
  const screenH = cfg?.screenH || 200
  const scale   = cfg?.scale   || 4
  const studioC = path.join(RUNTIME_DIR, 'studio.c')

  fs.mkdirSync(BUILD_DIR, { recursive: true })

  // remove any stale files from previous sessions
  const stale = ['ComicMono.ttf', 'ComicMono-Bold.ttf', 'PixelComicSans.ttf', 'Ac437_Acer_VGA_8x8.ttf']
  stale.forEach(f => { try { fs.unlinkSync(path.join(BUILD_DIR, f)) } catch {} })

  fs.writeFileSync(CART_SRC, code)
  if (fs.existsSync(FONT_SRC)) fs.copyFileSync(FONT_SRC, FONT_DEST)

  const args = [
    `"${CART_SRC}"`,
    `"${studioC}"`,
    `-I"${RUNTIME_DIR}"`,
    `-I"${RAYLIB}/include"`,
    `-DSCREEN_W=${screenW}`,
    `-DSCREEN_H=${screenH}`,
    `-DSCALE=${scale}`,
    `"${RAYLIB}/lib/libraylib.a"`,
    '-framework OpenGL',
    '-framework Cocoa',
    '-framework IOKit',
    '-framework CoreVideo',
    '-framework CoreFoundation',
    `-o "${CART_BIN}"`,
  ]

  const cmd = `clang ${args.join(' ')}`

  return new Promise(resolve => {
    exec(cmd, (err, _stdout, stderr) => {
      // filter out the noisy macOS version warnings — they're never actionable
      const warnings = stderr
        .split('\n')
        .filter(l => !l.includes('was built for newer macOS version'))
        .join('\n')
        .trim()

      if (err) return resolve({ ok: false, cmd, output: warnings })

      const proc = spawn(CART_BIN, [], { detached: true, stdio: 'ignore', cwd: BUILD_DIR })
      proc.unref()

      resolve({
        ok:     true,
        cmd,
        output: warnings || null,   // any remaining warnings
        src:    CART_SRC,
        bin:    CART_BIN,
      })
    })
  })
})
