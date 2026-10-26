#!/usr/bin/env python3
"""Cross compile the client for Linux and macOS with clang, lld and Ninja.

  python Tools/Cross/cross.py sysroot linux
  python Tools/Cross/cross.py sysroot macos
  python Tools/Cross/cross.py build linux [--target Wow] [--config RelWithDebInfo]
  python Tools/Cross/cross.py build macos-arm64
  python Tools/Cross/cross.py build macos-x86_64
  python Tools/Cross/cross.py package macos [--no-build]

The Linux sysroot and the macOS dependencies live in .cross/, the macOS SDK is the ThirdParty/macos-sdk submodule,
build trees go to build/cross-<name>/. Every build is named HoTClient64 (--name changes it), so Linux writes
bin/Linux/HoTClient64 and package builds both Mac architectures and writes a universal HoTClient64.app to
bin/HoTClient64-mac.tar.gz. WOWDECOMP_CROSS_DIR (read here and by the toolchains) moves .cross/ somewhere else.
"""

import argparse
import io
import json
import lzma
import os
import posixpath
import shutil
import struct
import subprocess
import sys
import tarfile
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CROSS = Path(os.environ.get("WOWDECOMP_CROSS_DIR", ROOT / ".cross"))
CACHE = CROSS / "cache"
CLIENT_NAME = "HoTClient64"

DEBIAN_MIRROR = "https://deb.debian.org/debian"
DEBIAN_SUITE = "bookworm"

LINUX_ROOTS = [
    "libc6-dev",
    "linux-libc-dev",
    "libgcc-12-dev",
    "libstdc++-12-dev",
    "libsdl2-dev",
    "libgl-dev",
    "libglx-dev",
    "libcurl4-openssl-dev",
    "libssl-dev",
    "libxvidcore-dev",
    "zlib1g-dev",
    "libudev-dev",
]

# Runtime tooling the dev packages depend on but nothing links against.
LINUX_SKIP = {
    "adduser", "dbus", "dbus-bin", "dbus-daemon", "dbus-session-bus-common", "dbus-system-bus-common", "dbus-user-session",
    "debconf", "dpkg", "dpkg-dev", "init-system-helpers", "install-info", "libc-bin", "libc-dev-bin", "lsb-base", "mount",
    "passwd", "pkg-config", "pkgconf", "pkgconf-bin", "procps", "sensible-utils", "systemd", "systemd-sysv", "tzdata", "ucf",
    "udev", "util-linux", "x11-common", "xkb-data", "libglib2.0-dev-bin", "libglib2.0-bin", "libglib2.0-data",
    "shared-mime-info", "gcc-12", "cpp-12", "binutils", "make", "patch", "xz-utils", "bzip2",
}
LINUX_SKIP_PREFIX = ("python3", "libpython3", "perl", "gir1.2-", "libgirepository")

# Only the parts of each package a compiler and linker read.
LINUX_KEEP = ("usr/include/", "usr/lib/", "usr/lib64/", "lib/", "lib64/", "usr/share/pkgconfig/")

BOTTLE_TAGS = {
    "arm64": ["arm64_big_sur", "arm64_monterey", "arm64_ventura", "arm64_sonoma", "arm64_sequoia", "arm64_tahoe", "arm64_golden_gate"],
    "x86_64": ["big_sur", "monterey", "ventura", "sonoma", "sequoia", "tahoe"],
}
# Only xvidcore: curl is in the SDK, and SHA-256/ECDSA use CommonCrypto and Security.framework on macOS.
BOTTLES = ["xvid"]


def log(msg):
    print(msg, flush=True)


def fetch(url, dest, headers=None):
    dest = Path(dest)

    if dest.exists():
        return dest

    dest.parent.mkdir(parents=True, exist_ok=True)
    log(f"  get {url}")
    request = urllib.request.Request(url, headers=headers or {})

    with urllib.request.urlopen(request) as response, open(str(dest) + ".part", "wb") as out:
        shutil.copyfileobj(response, out)

    os.replace(str(dest) + ".part", dest)
    return dest


def materialize_links(root, links):
    """Windows cannot create symlinks without developer mode, so every link becomes a copy of its target."""
    pending = list(links)

    while pending:
        remaining = []

        for link, target in pending:
            if target.startswith("/"):
                resolved = root / target.lstrip("/")
            else:
                resolved = (link.parent / target)

            resolved = Path(os.path.normpath(resolved))

            if not resolved.exists():
                remaining.append((link, target))
                continue

            if link.exists() or link.is_symlink():
                continue

            link.parent.mkdir(parents=True, exist_ok=True)

            if resolved.is_dir():
                shutil.copytree(resolved, link, dirs_exist_ok=True)
            else:
                shutil.copy2(resolved, link)

        if len(remaining) == len(pending):
            break

        pending = remaining

    return len(pending)


def extract_tar(data, dest, keep=None, strip=0, links=None):
    opened = tarfile.open(fileobj=io.BytesIO(data)) if isinstance(data, bytes) else tarfile.open(data)

    with opened as tar:
        for member in tar:
            name = member.name

            while name.startswith("./"):
                name = name[2:]

            parts = name.split("/")

            if len(parts) <= strip:
                continue

            name = "/".join(parts[strip:])

            if os.name == "nt" and any(c in name for c in ':*?"<>|'):
                continue

            if not name or (keep and not any((name + "/").startswith(k) or name.startswith(k) for k in keep)):
                continue

            target = dest / name

            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            elif member.issym() or member.islnk():
                if links is not None:
                    linkname = member.linkname

                    if member.islnk():
                        parts = linkname.lstrip("./").split("/")
                        linkname = "/" + "/".join(parts[strip:])

                    links.append((target, linkname))
            elif member.isfile():
                target.parent.mkdir(parents=True, exist_ok=True)

                with tar.extractfile(member) as src, open(target, "wb") as out:
                    shutil.copyfileobj(src, out)


def deb_data(path):
    with open(path, "rb") as f:
        blob = f.read()

    if not blob.startswith(b"!<arch>\n"):
        raise RuntimeError(f"{path} is not a deb")

    offset = 8

    while offset < len(blob):
        header = blob[offset:offset + 60]
        name = header[:16].decode().strip().rstrip("/")
        size = int(header[48:58].decode().strip())
        body = blob[offset + 60:offset + 60 + size]
        offset += 60 + size + (size & 1)

        if not name.startswith("data.tar"):
            continue

        if name.endswith(".xz"):
            return lzma.decompress(body)

        if name.endswith(".zst"):
            from compression import zstd
            return zstd.decompress(body)

        if name.endswith(".gz"):
            import gzip
            return gzip.decompress(body)

        return body

    raise RuntimeError(f"{path} has no data member")


def debian_index():
    path = fetch(f"{DEBIAN_MIRROR}/dists/{DEBIAN_SUITE}/main/binary-amd64/Packages.xz", CACHE / f"debian-{DEBIAN_SUITE}-Packages.xz")
    text = lzma.decompress(path.read_bytes()).decode("utf-8", "replace")
    packages = {}
    provides = {}

    for stanza in text.split("\n\n"):
        fields = {}
        key = None

        for line in stanza.splitlines():
            if line.startswith((" ", "\t")) and key:
                fields[key] += line
            elif ":" in line:
                key, value = line.split(":", 1)
                fields[key] = value.strip()

        name = fields.get("Package")

        if not name:
            continue

        packages[name] = fields

        for provided in fields.get("Provides", "").split(","):
            provided = provided.strip().split(" ")[0]

            if provided:
                provides.setdefault(provided, name)

    return packages, provides


def skipped(name):
    return name in LINUX_SKIP or name.startswith(LINUX_SKIP_PREFIX)


def resolve_debian(packages, provides, roots):
    chosen = {}
    queue = list(roots)

    while queue:
        name = queue.pop()

        if name in chosen or skipped(name):
            continue

        if name not in packages:
            if name in provides:
                queue.append(provides[name])
                continue

            raise RuntimeError(f"debian package {name} not found")

        fields = packages[name]
        chosen[name] = fields

        for field in ("Pre-Depends", "Depends"):
            for clause in fields.get(field, "").split(","):
                clause = clause.strip()

                if not clause:
                    continue

                for alternative in clause.split("|"):
                    dep = alternative.strip().split(" ")[0].split(":")[0]

                    if dep in packages or dep in provides:
                        queue.append(dep)
                        break

    return chosen


def sysroot_linux(args):
    dest = CROSS / "linux-x86_64"
    packages, provides = debian_index()
    chosen = resolve_debian(packages, provides, LINUX_ROOTS)
    log(f"linux sysroot: {len(chosen)} packages")

    if dest.exists():
        shutil.rmtree(dest)

    dest.mkdir(parents=True)
    links = []

    for name in sorted(chosen):
        filename = chosen[name]["Filename"]
        deb = fetch(f"{DEBIAN_MIRROR}/{filename}", CACHE / "debs" / posixpath.basename(filename))
        extract_tar(deb_data(deb), dest, keep=LINUX_KEEP, links=links)

    broken = materialize_links(dest, links)
    # sdl2-config.cmake checks that its bin folder exists.
    (dest / "usr" / "bin").mkdir(parents=True, exist_ok=True)
    log(f"linux sysroot ready at {dest} ({broken} dangling links skipped)")


MACOS_SDK_REPO = "https://github.com/alexey-lysiuk/macos-sdk"
MACOS_SDK_PATH = ROOT / "ThirdParty" / "macos-sdk"
MACOS_SDK_FOLDER = "MacOSX26.5.sdk"


def git(*args):
    # MSYS_NO_PATHCONV keeps Git for Windows from rewriting the leading / of the sparse patterns.
    subprocess.run(["git", *args], check=True, cwd=ROOT, env={**os.environ, "MSYS_NO_PATHCONV": "1"})


def init_macos_sdk():
    """ThirdParty/macos-sdk is a submodule with update = none: the repo holds every SDK since 10.1, so it is checked out
    sparse with only the one SDK the toolchain uses, minus the man pages and Catalyst/Swift parts nothing here reads."""
    sdk = MACOS_SDK_PATH / MACOS_SDK_FOLDER

    if (sdk / "usr" / "include" / "stdio.h").exists():
        return

    if not (MACOS_SDK_PATH / ".git").exists():
        git("submodule", "init", "ThirdParty/macos-sdk")
        commit = subprocess.run(["git", "-C", str(ROOT), "ls-tree", "HEAD", "ThirdParty/macos-sdk"], capture_output=True, text=True).stdout.split()
        git("clone", "--filter=blob:none", "--no-checkout", "--sparse", MACOS_SDK_REPO, str(MACOS_SDK_PATH))

        if len(commit) >= 3:
            git("-C", str(MACOS_SDK_PATH), "fetch", "--filter=blob:none", "origin", commit[2])
            git("-C", str(MACOS_SDK_PATH), "update-ref", "HEAD", commit[2])

    # The SDK's frameworks are built from symlinks, a few man pages have names NTFS refuses, and some paths pass MAX_PATH.
    git("-C", str(MACOS_SDK_PATH), "config", "core.symlinks", "true")
    git("-C", str(MACOS_SDK_PATH), "config", "core.protectNTFS", "false")
    git("-C", str(MACOS_SDK_PATH), "config", "core.longpaths", "true")
    git("-C", str(MACOS_SDK_PATH), "sparse-checkout", "set", "--no-cone", f"/{MACOS_SDK_FOLDER}/", f"!/{MACOS_SDK_FOLDER}/usr/share/", f"!/{MACOS_SDK_FOLDER}/System/Cryptexes/", f"!/{MACOS_SDK_FOLDER}/System/iOSSupport/")
    git("-C", str(MACOS_SDK_PATH), "checkout")

    if not (sdk / "usr" / "include" / "stdio.h").exists():
        raise RuntimeError(f"{sdk} is incomplete after the checkout")


def install_bottle(name, arch, dest):
    info = json.loads(urllib.request.urlopen(f"https://formulae.brew.sh/api/formula/{name}.json").read())
    files = info["bottle"]["stable"]["files"]
    tag = next((t for t in BOTTLE_TAGS[arch] if t in files), None)

    if not tag:
        raise RuntimeError(f"no {arch} bottle for {name}")

    entry = files[tag]
    version = info["versions"]["stable"]
    tarball = fetch(entry["url"], CACHE / "bottles" / f"{name}-{version}-{tag}.tar.gz", {"Authorization": "Bearer QQ=="})
    links = []
    extract_tar(str(tarball), dest, keep=("include/", "lib/"), strip=2, links=links)
    materialize_links(dest, links)
    log(f"  {name} {version} ({tag}) into {dest}")


def sysroot_macos(args):
    root = CROSS / "macos"
    root.mkdir(parents=True, exist_ok=True)

    init_macos_sdk()

    for arch in ("arm64", "x86_64"):
        deps = root / f"deps-{arch}"

        if deps.exists():
            shutil.rmtree(deps)

        for name in BOTTLES:
            install_bottle(name, arch, deps)

        # The bottles ship both, link the archives so the client carries no Homebrew paths.
        for dylib in list((deps / "lib").glob("*.dylib")):
            dylib.unlink()

    log(f"macos sysroot ready at {root}")


def find_ninja():
    path = shutil.which("ninja")

    if path:
        return path

    vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    roots = []

    if vswhere.exists():
        out = subprocess.run([str(vswhere), "-all", "-property", "installationPath"], capture_output=True, text=True).stdout
        roots += [Path(line.strip()) for line in out.splitlines() if line.strip()]

    for root in roots:
        candidate = root / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" / "Ninja" / "ninja.exe"

        if candidate.exists():
            return str(candidate)

    raise RuntimeError("ninja not found on PATH or in a Visual Studio install")


TARGETS = {
    "linux": ("linux-x86_64.cmake", [f"-DWOWDECOMP_SYSROOT={(CROSS / 'linux-x86_64').as_posix()}"]),
    "macos-arm64": ("macos.cmake", ["-DWOWDECOMP_MACOS_ARCH=arm64", f"-DWOWDECOMP_CROSS_ROOT={(CROSS / 'macos').as_posix()}"]),
    "macos-x86_64": ("macos.cmake", ["-DWOWDECOMP_MACOS_ARCH=x86_64", f"-DWOWDECOMP_CROSS_ROOT={(CROSS / 'macos').as_posix()}"]),
}


def build(args):
    toolchain, extra = TARGETS[args.name]
    build_dir = Path(args.build_dir) if args.build_dir else ROOT / "build" / f"cross-{args.name}"
    ninja = find_ninja()

    if args.fresh and build_dir.exists():
        shutil.rmtree(build_dir)

    configure = [
        "cmake", "-S", str(ROOT), "-B", str(build_dir), "-G", "Ninja",
        f"-DCMAKE_MAKE_PROGRAM={ninja}",
        f"-DCMAKE_TOOLCHAIN_FILE={ROOT / 'cmake' / 'toolchains' / toolchain}",
        f"-DCMAKE_BUILD_TYPE={args.config}",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        f"-DWOW_OUTPUT_OVERRIDE={args.name_override}",
    ] + extra + args.cmake_args

    if subprocess.run(configure).returncode != 0:
        sys.exit(1)

    if args.configure_only:
        return

    command = ["cmake", "--build", str(build_dir)]

    if args.target:
        command += ["--target", args.target]

    command += ["--", "-k", str(args.keep_going)]
    sys.exit(subprocess.run(command).returncode)


MACOS_TESTER_README = """{name} for macOS (universal, Apple Silicon and Intel)

1. Put {name}.app in your World of Warcraft 3.3.5a folder, next to Data and WTF.
2. The app is not notarized, so macOS blocks the first launch. Run this once in Terminal from that folder:
       xattr -dr com.apple.quarantine {name}.app
   or open it, then allow it under System Settings > Privacy & Security > Open Anyway.
3. Crash logs are written to the Errors folder next to the app.
"""


def universal_binary(slices):
    """The fat Mach-O lipo -create writes. Each slice keeps its own code signature."""
    align = 14
    header = struct.pack(">II", 0xCAFEBABE, len(slices))
    offset = 1 << align
    entries = []
    body = b""

    for data in slices:
        cputype, cpusubtype = struct.unpack_from("<Ii", data, 4)
        entries.append(struct.pack(">IiIII", cputype, cpusubtype, offset, len(data), align))
        padded = data + b"\x00" * (-len(data) % (1 << align))
        body += padded
        offset += len(padded)

    head = header + b"".join(entries)
    return head + b"\x00" * ((1 << align) - len(head)) + body


def tar_add(tar, name, data=None, mode=0o644, link=None, directory=False):
    info = tarfile.TarInfo(name)
    info.mtime = int(time.time())

    if directory:
        info.type = tarfile.DIRTYPE
        info.mode = 0o755
        tar.addfile(info)
    elif link is not None:
        info.type = tarfile.SYMTYPE
        info.linkname = link.replace("\\", "/")
        info.mode = 0o755
        tar.addfile(info)
    else:
        info.size = len(data)
        info.mode = mode
        tar.addfile(info, io.BytesIO(data))


def package_macos(args):
    archs = ["arm64", "x86_64"]

    if not args.no_build:
        for arch in archs:
            if subprocess.run([sys.executable, __file__, "build", f"macos-{arch}", "--name", args.name_override]).returncode != 0:
                sys.exit(1)

    name = args.name_override
    apps = [ROOT / "bin" / f"Mac-{arch}" / f"{name}.app" for arch in archs]

    for app in apps:
        if not (app / "Contents" / "MacOS" / name).exists():
            sys.exit(f"missing {app}, build it first")

    binary = universal_binary([(app / "Contents" / "MacOS" / name).read_bytes() for app in apps])
    out = ROOT / "bin" / f"{name}-mac.tar.gz"

    with tarfile.open(out, "w:gz", format=tarfile.PAX_FORMAT) as tar:
        tar_add(tar, name, directory=True)
        tar_add(tar, f"{name}/README.txt", MACOS_TESTER_README.format(name=name).encode())
        tar_add(tar, f"{name}/{name}.app", directory=True)
        tar_add(tar, f"{name}/{name}.app/Contents", directory=True)
        tar_add(tar, f"{name}/{name}.app/Contents/Info.plist", (apps[0] / "Contents" / "Info.plist").read_bytes())
        tar_add(tar, f"{name}/{name}.app/Contents/MacOS", directory=True)
        tar_add(tar, f"{name}/{name}.app/Contents/MacOS/{name}", binary, 0o755)
        tar_add(tar, f"{name}/{name}.app/Contents/Resources", directory=True)

        for resource in sorted((apps[0] / "Contents" / "Resources").iterdir()):
            tar_add(tar, f"{name}/{name}.app/Contents/Resources/{resource.name}", resource.read_bytes())

    log(f"  {out} ({out.stat().st_size // 1048576} MB)")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    sysroot = sub.add_parser("sysroot")
    sysroot.add_argument("platform", choices=["linux", "macos"])

    b = sub.add_parser("build")
    b.add_argument("name", choices=sorted(TARGETS))
    b.add_argument("--target", default="Wow")
    b.add_argument("--config", default="RelWithDebInfo")
    b.add_argument("--fresh", action="store_true", help="delete the build tree first")
    b.add_argument("--build-dir", help="build tree, default build/cross-<name>")
    b.add_argument("--configure-only", action="store_true")
    b.add_argument("--keep-going", type=int, default=1, help="ninja -k, 0 keeps going past every failure")
    b.add_argument("--name", dest="name_override", default=CLIENT_NAME, help=f"executable and bundle name, default {CLIENT_NAME}")
    b.add_argument("cmake_args", nargs="*", help="extra -D options for the configure step")

    p = sub.add_parser("package")
    p.add_argument("platform", choices=["macos"])
    p.add_argument("--no-build", action="store_true", help="package the existing bin/Mac-<arch> bundles")
    p.add_argument("--name", dest="name_override", default=CLIENT_NAME, help=f"executable and bundle name, default {CLIENT_NAME}")

    args = parser.parse_args()

    if args.command == "sysroot":
        (sysroot_linux if args.platform == "linux" else sysroot_macos)(args)
    elif args.command == "package":
        package_macos(args)
    else:
        build(args)


if __name__ == "__main__":
    main()
