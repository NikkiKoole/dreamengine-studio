const { app, BrowserWindow, ipcMain, Menu, session } = require('electron')
const { exec, execSync, spawn }        = require('child_process')
const path                             = require('path')
const fs                               = require('fs')

const RUNTIME_DIR = path.join(__dirname, '../../runtime')
const BUILD_DIR   = path.join(__dirname, '../../build')
const RAYLIB      = '/usr/local/opt/raylib'
const RAYLIB_WIN  = '/usr/local/opt/raylib-win64/raylib-5.5_win64_mingw-w64'
const MINGW       = 'x86_64-w64-mingw32-gcc'
const CART_SRC    = path.join(BUILD_DIR, 'cart.c')
const CART_BIN    = path.join(BUILD_DIR, 'cart')
const CART_EXE    = path.join(BUILD_DIR, 'cart.exe')

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


// ── sprites handler ───────────────────────────────────────────
ipcMain.handle('studio:save-sprites', (_event, dataUrl) => {
  fs.mkdirSync(BUILD_DIR, { recursive: true })
  const base64 = dataUrl.replace(/^data:image\/png;base64,/, '')
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

  // generate sprites_data.h from sprites.png via xxd (PNG is compressed — stays small)
  const spritesHeader = path.join(BUILD_DIR, 'sprites_data.h')
  const spritesPng    = path.join(BUILD_DIR, 'sprites.png')
  if (fs.existsSync(spritesPng)) {
    try {
      const xxd = execSync('xxd -i sprites.png', { cwd: BUILD_DIR }).toString()
      fs.writeFileSync(spritesHeader,
        xxd.replace(/unsigned char sprites_png\[\]/,  'static const unsigned char SPRITES_DATA[]')
           .replace(/unsigned int sprites_png_len/,   'static const unsigned int  SPRITES_DATA_LEN'))
    } catch {
      fs.writeFileSync(spritesHeader, 'static const unsigned char SPRITES_DATA[]={0};static const unsigned int SPRITES_DATA_LEN=0;\n')
    }
  } else {
    fs.writeFileSync(spritesHeader, 'static const unsigned char SPRITES_DATA[]={0};static const unsigned int SPRITES_DATA_LEN=0;\n')
  }

  const args = [
    `"${CART_SRC}"`,
    `"${studioC}"`,
    `-I"${RUNTIME_DIR}"`,
    `-I"${BUILD_DIR}"`,
    `-I"${RAYLIB}/include"`,
    `-DSCREEN_W=${screenW}`,
    `-DSCREEN_H=${screenH}`,
    `-DSCALE=${scale}`,
    '-Os',
    `"${RAYLIB}/lib/libraylib.a"`,
    '-framework OpenGL',
    '-framework Cocoa',
    '-framework IOKit',
    '-framework CoreVideo',
    '-framework CoreFoundation',
    '-Wl,-dead_strip',
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

      // also cross-compile for Windows in the background
      if (fs.existsSync(MINGW) || fs.existsSync(`/usr/local/bin/${MINGW}`)) {
        const winArgs = [
          `"${CART_SRC}"`, `"${studioC}"`,
          `-I"${RUNTIME_DIR}"`, `-I"${BUILD_DIR}"`, `-I"${RAYLIB_WIN}/include"`,
          '-Os',
          `"${RAYLIB_WIN}/lib/libraylib.a"`,
          '-lopengl32', '-lgdi32', '-lwinmm', '-lcomdlg32',
          '-Wl,--gc-sections',
          `-o "${CART_EXE}"`,
        ]
        exec(`${MINGW} ${winArgs.join(' ')}`, () => {})
      }

      resolve({
        ok:     true,
        cmd,
        output: warnings || null,
        src:    CART_SRC,
        bin:    CART_BIN,
        exe:    CART_EXE,
      })
    })
  })
})
