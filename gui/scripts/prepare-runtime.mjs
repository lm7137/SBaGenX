import fs from 'node:fs'
import path from 'node:path'

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

function stageLinux() {
  const src = newestLinuxRuntime()
  copyFile(src, 'libsbagenx.so.3')
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
