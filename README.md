# KidoDarwin

KidoDarwin is a small, open-source Darwin system for Apple Silicon. It boots Apple's XNU kernel in a
virtual machine and runs macOS command-line tools. [kido](https://github.com/kido-sh/kido) uses it
as the system inside every container.

```console
$ kido run --rm ghcr.io/kido-sh/darwin sw_vers
ProductName:	KidoDarwin
ProductVersion:	26.5
BuildVersion:	25F74
$ kido run --rm ghcr.io/kido-sh/darwin uname -a
Darwin kido 25.5.0 Darwin Kernel Version 25.5.0: Mon Nov 23 13:26:52 UTC 2026; nixbld1:xnu-12377.121.6~1/RELEASE_ARM64_VIRT arm64
```

## What's in it

- XNU from Apple's open-source release for macOS 26.5, with the drivers a virtual machine needs.
- launchd, dyld and libSystem.
- A small set of Unix tools (toybox and a few BSD tools), zsh as `/bin/sh`, and vi.
- libcurl, libedit and ncurses with its terminfo database, as macOS has them in `/usr/lib`.
- kido's guest agent, which runs the commands that kido sends.
- The licenses of all of the above, in `/usr/share/doc/kido`.

The image is about 40 MB to download. A `kido run` of one command takes under a second.

## Run your own tools

macOS command-line tools run unchanged, as long as KidoDarwin has the functions and libraries they
use. Like macOS, it runs arm64 and arm64e tools on arm64e system libraries: dyld, libSystem,
libobjc, libc++ and the libraries Apple's own /bin/ls and /usr/bin/grep link are arm64e. Its other
libraries are still arm64 only, which arm64e tools cannot load.
[kido's README](https://github.com/kido-sh/kido#examples) shows how to build an image with your
own tool in it.

## Build it

The build uses Nix with flakes on aarch64 Linux. On a Mac, run it in a Linux VM or container that
has Nix. It builds everything from source.

```sh
nix build .#oci                  # the image, as an OCI image layout
tar -C result -cf darwin.tar .
kido load -i darwin.tar          # on the Mac: loads it as darwin:latest
```

`.#image` builds the same image as a plain directory with `kernel`, `initrd.cpio`, `disk.img` and
`image.json`. `kido image import` reads that directory.

## What's where

| Path | What it holds |
|---|---|
| `darwin/` | The system's source: XNU, libSystem, launchd, dyld and the rest |
| `loader/` | The loader. QEMU and Virtualization.framework boot it like a Linux kernel, and it starts XNU |
| `kc-tools/`, `iig-tools/` | Tools that build the kernel collection and the IOKit interfaces |
| `nix/` | The build |
| `LICENSING.md`, `LICENSES/`, `NOTICE` | Which license covers which files, and the license texts |

## Security

The corecrypto in KidoDarwin is a fork that nobody has audited. Do not rely on the guest's crypto
library to protect anything that matters.

## Where it comes from

KidoDarwin started from [PureDarwin](https://github.com/PureDarwin/PureDarwin) and from
[xnu-loader](https://github.com/PureDarwin/xnu-loader) by Vali0004. Most of its code comes from
Apple's [open-source releases](https://opensource.apple.com/). Its corecrypto is based on
NeoDarwin's [corecrypto](https://github.com/NeoDarwinOS/corecrypto). Thank you all.
[docs/upstream.md](docs/upstream.md) lists each upstream and every file we changed.

KidoDarwin is an independent project. It is not affiliated with Apple, PureDarwin or NeoDarwin.

## License

Every file keeps the license it came with. Most of Apple's code is under the Apple Public Source
License 2.0. PureDarwin and xnu-loader use BSD licenses, and corecrypto uses MPL-2.0. kido's own
files are under the Apache License 2.0. [LICENSING.md](LICENSING.md) says which license covers which files.
