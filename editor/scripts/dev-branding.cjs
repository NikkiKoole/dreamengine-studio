#!/usr/bin/env node
// Re-brand the dev Electron bundle so Cmd-Tab and the Dock show "dreamengine"
// instead of "Electron". macOS reads these from the bundle's Info.plist *before*
// our JS runs, so app.setName() can't fix them — we patch the plist directly.
//
// This lives in node_modules and gets wiped by `npm install`, so it runs as a
// `postinstall` hook and again before `npm start` (see package.json). macOS-only;
// a no-op everywhere else.
const { execFileSync } = require('child_process')
const fs = require('fs')
const path = require('path')

const NAME = 'dreamengine'

if (process.platform !== 'darwin') process.exit(0)

const plist = path.join(__dirname, '..', 'node_modules', 'electron', 'dist',
                        'Electron.app', 'Contents', 'Info.plist')

if (!fs.existsSync(plist)) {
  // electron not installed yet (e.g. postinstall ordering) — nothing to do.
  process.exit(0)
}

try {
  for (const key of ['CFBundleName', 'CFBundleDisplayName']) {
    execFileSync('/usr/libexec/PlistBuddy', ['-c', `Set :${key} ${NAME}`, plist])
  }
  console.log(`[dev-branding] Electron bundle renamed to "${NAME}"`)
} catch (err) {
  console.warn('[dev-branding] could not patch Info.plist:', err.message)
}
