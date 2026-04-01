import fs from 'node:fs'
import path from 'node:path'
import { execSync } from 'node:child_process'

const repoRoot = path.resolve(process.cwd(), '..')
const distDir = path.join(repoRoot, 'dist')
const bundleDir = path.join(process.cwd(), 'src-tauri', 'runtime-bundle')

function resetDir(dir) {
  fs.rmSync(dir, { recursive: true, force: true })
  fs.mkdirSync(dir, { recursive: true })
}

function copyFile(src, destName = path.basename(src)) {
  const dest = path.join(bundleDir, destName)
  fs.copyFileSync(src, dest)
  return dest
}

function parseStableVersion(name, prefix) {
  if (!name.startsWith(prefix)) return null
  const version = name.slice(prefix.length)
  if (!/^\d+\.\d+\.\d+$/.test(version)) return null
  return version.split('.').map((part) => Number.parseInt(part, 10))
}

function compareVersions(a, b) {
  for (let i = 0; i < Math.max(a.length, b.length); i += 1) {
    const av = a[i] ?? 0
    const bv = b[i] ?? 0
    if (av !== bv) return av - bv
  }
  return 0
}

function newestLinuxRuntime() {
  const activeSoname = path.join(distDir, 'libsbagenx.so.3')
  if (fs.existsSync(activeSoname)) {
    return fs.realpathSync(activeSoname)
  }

  const prefix = 'libsbagenx.so.'
  const entries = fs
    .readdirSync(distDir, { withFileTypes: true })
    .filter((entry) => entry.isFile())
    .map((entry) => entry.name)
    .map((name) => ({ name, version: parseStableVersion(name, prefix) }))
    .filter((entry) => entry.version !== null)
    .sort((left, right) => compareVersions(left.version, right.version))
  const latest = entries.at(-1)
  if (!latest) {
    throw new Error('no stable libsbagenx.so.X.Y.Z runtime was found in dist/')
  }
  return path.join(distDir, latest.name)
}

function linuxArchHints() {
  switch (process.arch) {
    case 'x64':
      return [/x86-64/i, /x86_64-linux-gnu/i]
    case 'ia32':
      return [/i386/i, /i686/i, /i386-linux-gnu/i]
    case 'arm64':
      return [/aarch64/i, /arm64/i, /aarch64-linux-gnu/i]
    default:
      return []
  }
}

function resolveLinuxRuntimeLib(soname) {
  const hints = linuxArchHints()
  try {
    const output = execSync('ldconfig -p', { encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] })
    const candidates = output
      .split('\n')
      .map((line) => line.trim())
      .filter((line) => line.startsWith(`${soname} `) && line.includes(' => '))
      .map((line) => {
        const resolved = line.replace(/^.*=>\s*/, '').trim()
        const score = hints.some((hint) => hint.test(line)) ? 1 : 0
        return { resolved, score }
      })
      .sort((left, right) => right.score - left.score)
    if (candidates.length > 0) {
      return fs.realpathSync(candidates[0].resolved)
    }
  } catch {
    // Fall back to well-known library locations below.
  }

  const fallbackDirs = [
    '/lib',
    '/usr/lib',
    '/lib64',
    '/usr/lib64',
    '/lib/x86_64-linux-gnu',
    '/usr/lib/x86_64-linux-gnu',
    '/lib/i386-linux-gnu',
    '/usr/lib/i386-linux-gnu',
    '/lib/aarch64-linux-gnu',
    '/usr/lib/aarch64-linux-gnu',
  ]
  for (const dir of fallbackDirs) {
    const candidate = path.join(dir, soname)
    if (fs.existsSync(candidate)) {
      return fs.realpathSync(candidate)
    }
  }
  throw new Error(`required Linux runtime dependency is missing: ${soname}`)
}

function stageLinux() {
  const src = newestLinuxRuntime()
  copyFile(src, 'libsbagenx.so.3')

  const required = [
    'libsndfile.so.1',
    'libmp3lame.so.0',
    'libFLAC.so.12',
    'libmpg123.so.0',
    'libogg.so.0',
    'libvorbis.so.0',
    'libvorbisenc.so.2',
    'libopus.so.0',
  ]

  for (const soname of required) {
    copyFile(resolveLinuxRuntimeLib(soname), soname)
  }
}

function stageWindows() {
  const is64 = process.arch === 'x64' || process.arch === 'arm64'
  const suffix = is64 ? 'win64' : 'win32'
  const required = [
    `sbagenxlib-${suffix}.dll`,
    `libsndfile-${suffix}.dll`,
    `libmp3lame-${suffix}.dll`,
    `libogg-0-${suffix}.dll`,
    `libvorbis-0-${suffix}.dll`,
    `libvorbisenc-2-${suffix}.dll`,
    `libFLAC-${suffix}.dll`,
    `libmpg123-0-${suffix}.dll`,
    `libopus-0-${suffix}.dll`,
    `libwinpthread-1-${suffix}.dll`,
  ]

  for (const file of required) {
    const src = path.join(distDir, file)
    if (!fs.existsSync(src)) {
      throw new Error(`required Windows runtime dependency is missing: ${src}`)
    }
    copyFile(src)
  }

  const webview2Loader = path.join(process.cwd(), 'src-tauri', 'target', 'release', 'WebView2Loader.dll')
  if (fs.existsSync(webview2Loader)) {
    copyFile(webview2Loader, 'WebView2Loader.dll')
  }
}

function stageMacos() {
  const src = path.join(distDir, 'libsbagenx.dylib')
  if (!fs.existsSync(src)) {
    throw new Error(`required macOS runtime dependency is missing: ${src}`)
  }
  copyFile(src)
}

function main() {
  resetDir(bundleDir)

  switch (process.platform) {
    case 'linux':
      stageLinux()
      break
    case 'win32':
      stageWindows()
      break
    case 'darwin':
      stageMacos()
      break
    default:
      throw new Error(`unsupported packaging host platform: ${process.platform}`)
  }

  const staged = fs.readdirSync(bundleDir).sort()
  console.log(`Staged ${staged.length} runtime file(s) in ${bundleDir}`)
  for (const file of staged) {
    console.log(`- ${file}`)
  }
}

main()
