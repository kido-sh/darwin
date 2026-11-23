# The root filesystem as the disk installs it: the base system, the extra packages and the files the
# container needs in /etc, /var and /usr/libexec.
{ stdenv
, lib
, baseSystem
, extraPackages ? [ ]
, iana-etc
, cacert
# /usr/share/doc/kido: the source pointer and the license texts
, notices
}:

assert lib.isDerivation baseSystem;
assert lib.all lib.isDerivation extraPackages;

stdenv.mkDerivation {
  pname = "kido-system-tree";
  version = "0.1";
  dontUnpack = true;
  dontFixup = true;
  installPhase = ''
    runHook preInstall
    staging="$out"
    mkdir -p "$staging"

    copyPackage() {
      local package="$1"

      echo "Installing $package into image staging tree"

      if [ ! -d "$package" ]; then
        echo "error: package path does not exist or is not a directory: $package" >&2
        exit 1
      fi

      # A package may ship a top-level symlink (libiconv's include -> usr/include)
      # purely so build-time consumers can say -I$dep/include. cp -a cannot put
      # that over the staging tree's real directory, and it is not runtime
      # content anyway, so skip those rather than fail the image.
      local entry base
      for entry in "$package"/* "$package"/.[!.]*; do
        [ -e "$entry" ] || continue
        base="$(basename "$entry")"
        if [ -L "$entry" ] && [ -d "$staging/$base" ] && [ ! -L "$staging/$base" ]; then
          echo "  skipping $base (symlink would clobber staged directory)"
          continue
        fi
        if [ -d "$entry" ] && [ -L "$staging/$base" ]; then
          echo "  replacing $base symlink with staged directory"
          rm "$staging/$base"
        fi
        # replace files and never write through a staged symlink (framework-compat's CoreFoundation
        # binary lands on corefoundation's link to /usr/lib/libCoreFoundation.dylib)
        cp -a --remove-destination -- "$entry" "$staging"/
      done
      chmod -R u+rwX "$staging"
    }

    copyPackage "${baseSystem}";

    ${lib.concatMapStringsSep "\n" (package: ''
      copyPackage "${package}";
    '') extraPackages}
    for dir in \
      usr \
      usr/bin \
      usr/lib \
      usr/lib/system \
      usr/sbin \
      usr/share \
      bin \
      sbin \
      boot \
      dev \
      etc \
      private/tmp \
      private/var/tmp \
      var \
      var/cache \
      var/root \
      var/root/.cache \
      var/run \
      var/db \
      var/db/dhcpclient \
      var/db/dhcpclient/leases \
      var/log \
      var/empty
    do
      mkdir -p "$staging/$dir"
    done

    # macOS layout: the real temporary directories live under /private, with
    # /tmp and /var/tmp as symlinks. Nix's sandbox allows /private/tmp, and the
    # kernel resolves paths through the symlink before checking them.
    for tmpdir in tmp:private/tmp var/tmp:../private/var/tmp; do
      link="$staging/''${tmpdir%%:*}"
      target="''${tmpdir#*:}"
      if [ -d "$link" ] && [ ! -L "$link" ]; then
        cp -a "$link"/. "$(dirname "$link")/$target"/
        rm -rf "$link"
      fi
      ln -sfn "$target" "$link"
    done

    # macOS binaries link Foundation as Versions/C, and some software reads /etc through /private/etc
    foundation="$staging/System/Library/Frameworks/Foundation.framework/Versions"
    if [ -d "$foundation/A" ] && [ ! -e "$foundation/C" ]; then
      ln -s A "$foundation/C"
    fi
    [ -e "$staging/private/etc" ] || ln -s ../etc "$staging/private/etc"

    for dylib in "$staging"/lib/*.dylib; do
      [ -e "$dylib" ] || continue
      base=$(basename "$dylib")
      [ -e "$staging/usr/lib/$base" ] || ln -s "../../lib/$base" "$staging/usr/lib/$base"
    done

    for compat in libc libm libpthread libdl libinfo; do
      ln -sf libSystem.B.dylib "$staging/usr/lib/$compat.dylib"
    done
    # libresolv and libbsm are built into libSystem, clients linked against the separate
    # macOS dylibs find their symbols through these names
    for compat in libresolv.9 libresolv libbsm.0 libbsm; do
      ln -sf libSystem.B.dylib "$staging/usr/lib/$compat.dylib"
    done

    # headers, static libraries, pkg-config files and the kernel's link kit: nothing compiles in a container
    rm -rf "$staging/pd-guest-headers"
    # DriverKit, Kernel and System hold nothing but headers
    rm -rf "$staging/include" "$staging/usr/include" "$staging/usr/local/include" \
      "$staging"/System/Library/Frameworks/{DriverKit,Kernel,System}.framework \
      "$staging"/System/Library/Kernels/*.link
    find "$staging/System/Library/Frameworks" \( -name Headers -o -name PrivateHeaders \) -prune -exec rm -rf {} +
    find "$staging" -type d -name pkgconfig -prune -exec rm -rf {} +
    find "$staging" \( -name '*.a' -o -name '*.la' \) ! -type d -delete
    # no man, info or doc reader either
    rm -rf "$staging"/usr/share/{man,info,doc} "$staging"/share/{man,info,doc}
    cp -a ${notices}/usr/share/doc "$staging/usr/share/"
    chmod -R u+rwX "$staging/usr/share/doc"

    cat > $staging/etc/passwd <<'EOF'
root:*:0:0:System Administrator:/var/root:/bin/sh
daemon:*:1:1:System Services:/var/root:/usr/bin/false
sshd:*:74:74:Privilege-separated SSH:/var/empty:/usr/bin/false
messagebus:*:96:96:D-Bus Message Daemon User:/var/empty:/usr/bin/false
_trustd:*:282:282:trustd:/var/empty:/usr/bin/false
nobody:*:-2:-2:Unprivileged User:/var/empty:/usr/bin/false
EOF
    # Darwin's libinfo file_module reads /etc/master.passwd when euid==0 (it is
    # the root-only file that carries the hashes) and does NOT fall back to
    # /etc/passwd, so without this every getpwnam/getpwuid fails for root -
    # which breaks dbus, login and anything else resolving a user.
    # Format is the BSD master.passwd one: the two extra fields (class, change,
    # expire) sit between gid and gecos.
    cat > $staging/etc/master.passwd <<'EOF'
root:*:0:0::0:0:System Administrator:/var/root:/bin/sh
daemon:*:1:1::0:0:System Services:/var/root:/usr/bin/false
sshd:*:74:74::0:0:Privilege-separated SSH:/var/empty:/usr/bin/false
messagebus:*:96:96::0:0:D-Bus Message Daemon User:/var/empty:/usr/bin/false
_trustd:*:282:282::0:0:trustd:/var/empty:/usr/bin/false
nobody:*:-2:-2::0:0:Unprivileged User:/var/empty:/usr/bin/false
EOF
    chmod 600 $staging/etc/master.passwd

    cat > $staging/etc/group <<'EOF'
wheel:*:0:root
daemon:*:1:root
sshd:*:74:
messagebus:*:96:
_trustd:*:282:
nogroup:*:-1:
nobody:*:-2:
staff:*:20:root
EOF

    cp ${iana-etc}/etc/services $staging/etc/services
    cp ${iana-etc}/etc/protocols $staging/etc/protocols
    cat > $staging/etc/hosts <<'EOF'
127.0.0.1	localhost kido
::1		localhost kido
EOF
    cat > $staging/etc/fstab <<'EOF'
#
# Static filesystem mount table. The root filesystem is mounted by the kernel
# using the boot-uuid/boot-args, so it deliberately has no entry here.
#
EOF
    cat > $staging/etc/profile <<'EOF'
export PATH=/bin:/sbin:/usr/bin:/usr/sbin
export TERM=''${TERM:-vt220}
export SHELL=''${SHELL:-/bin/zsh}
export HOME=''${HOME:-/var/root}
export USER=''${USER:-root}
export LOGNAME=''${LOGNAME:-root}
export PS1='# '
EOF
    cat > $staging/etc/zshenv <<'EOF'
export PATH=''${PATH:-/bin:/sbin:/usr/bin:/usr/sbin}
export TERM=''${TERM:-vt220}
export SHELL=''${SHELL:-/bin/zsh}
export HOME=''${HOME:-/var/root}
export USER=''${USER:-root}
export LOGNAME=''${LOGNAME:-root}
EOF
    cat > $staging/etc/zprofile <<'EOF'
test -r /etc/profile && . /etc/profile
EOF
    cat > $staging/etc/zshrc <<'EOF'
# /etc/zshrc: interactive zsh setup for every user
[[ -o interactive ]] || return

# history, appended as commands run so concurrent shells keep each other's lines
HISTFILE=''${ZDOTDIR:-$HOME}/.zsh_history
HISTSIZE=10000
SAVEHIST=10000
setopt extended_history inc_append_history hist_ignore_dups hist_ignore_space hist_reduce_blanks
setopt interactive_comments no_beep

# emacs editing, plus what terminals send for the editing keys in either cursor-key mode
bindkey -e
bindkey '^?' backward-delete-char
bindkey '^H' backward-delete-char
bindkey '^[[3~' delete-char
bindkey '^[[3;5~' kill-word
bindkey '^[[2~' overwrite-mode
bindkey '^[[Z' reverse-menu-complete
bindkey '^[[5~' beginning-of-buffer-or-history
bindkey '^[[6~' end-of-buffer-or-history
autoload -Uz up-line-or-beginning-search down-line-or-beginning-search
zle -N up-line-or-beginning-search
zle -N down-line-or-beginning-search
for seq in '^[[A' '^[OA'; do bindkey $seq up-line-or-beginning-search; done
for seq in '^[[B' '^[OB'; do bindkey $seq down-line-or-beginning-search; done
for seq in '^[[D' '^[OD'; do bindkey $seq backward-char; done
for seq in '^[[C' '^[OC'; do bindkey $seq forward-char; done
for seq in '^[[H' '^[OH' '^[[1~' '^[[7~'; do bindkey $seq beginning-of-line; done
for seq in '^[[F' '^[OF' '^[[4~' '^[[8~'; do bindkey $seq end-of-line; done
for seq in '^[[1;5D' '^[[1;3D' '^[^[[D'; do bindkey $seq backward-word; done
for seq in '^[[1;5C' '^[[1;3C' '^[^[[C'; do bindkey $seq forward-word; done
unset seq

# colours for ls and grep, BSD and GNU-style tools alike
export CLICOLOR=1
export LS_COLORS='di=1;34:ln=1;36:so=1;35:pi=33:ex=1;32:bd=1;33:cd=1;33:su=37;41:sg=30;43:tw=30;42:ow=34;42'
ls --color=auto -d / >/dev/null 2>&1 && alias ls='ls --color=auto'
grep --color=auto -q x <<< x 2>/dev/null && alias grep='grep --color=auto'

# completion: menu selection, case-insensitive, coloured like ls
autoload -Uz compinit
# start from the dump made with the image, compinit rebuilds it when fpath has changed since
_zcompdump=''${ZDOTDIR:-$HOME}/.zcompdump
[[ -e $_zcompdump ]] || cp /usr/share/zsh/$ZSH_VERSION/zcompdump $_zcompdump 2>/dev/null
compinit -d $_zcompdump
unset _zcompdump
zmodload -i zsh/complist
zstyle ':completion:*' menu select
zstyle ':completion:*' matcher-list 'm:{a-zA-Z}={A-Za-z}'
zstyle ':completion:*' list-colors ''${(s.:.)LS_COLORS}
zstyle ':completion:*:descriptions' format '%B%d%b'

# the window title follows the directory, and names the command while it runs
if [[ $TERM == (xterm*|screen*|tmux*) ]]; then
  autoload -Uz add-zsh-hook
  _title_precmd() { print -Pn '\e]0;%n@%m: %~\a' }
  _title_preexec() { print -Pn '\e]0;%n@%m: '; print -rn -- $1; print -n '\a' }
  add-zsh-hook precmd _title_precmd
  add-zsh-hook preexec _title_preexec
fi

PROMPT='%F{green}%n@%m%f %B%F{blue}%1~%f%b %# '
EOF
    cat > $staging/etc/resolv.conf <<'EOF'
nameserver 10.0.2.3
EOF
    cat > $staging/etc/shells <<'EOF'
/bin/sh
/bin/zsh
EOF
    cat > $staging/etc/ttys <<'EOF'
/dev/console /bin/zsh -l
EOF
    mkdir -p $staging/System/Library/LaunchDaemons $staging/usr/libexec

    cat > $staging/usr/libexec/kido-set-hostname <<'EOF'
#!/bin/sh
if test -x /bin/hostname; then
    /bin/hostname kido
fi
EOF
    cat > $staging/usr/libexec/kido-net-up <<'EOF'
#!/bin/sh
# Sequential source ports repeat after a restart, while peers may still hold the old connections.
/bin/sysctl -w net.inet.tcp.randomize_ports=1 >/dev/null
case " $(/bin/sysctl -n kern.bootargs) " in
*" kido_ip="*)
    # launchd may have started mdnsd on the old resolv.conf; SIGHUP makes it take the new servers
    resolv=$(/bin/cat /etc/resolv.conf 2>/dev/null)
    /bin/netsetup --from-bootargs &&
        [ "$(/bin/cat /etc/resolv.conf 2>/dev/null)" != "$resolv" ] && /usr/bin/killall -HUP mdnsd
    ;;
esac
EOF
    chmod +x $staging/usr/libexec/kido-net-up
    cat > $staging/System/Library/LaunchDaemons/sh.kido.net-up.plist <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>Label</key>
	<string>sh.kido.net-up</string>
	<key>ProgramArguments</key>
	<array>
		<string>/usr/libexec/kido-net-up</string>
	</array>
	<key>RunAtLoad</key>
	<true/>
	<key>StandardOutPath</key>
	<string>/dev/console</string>
	<key>StandardErrorPath</key>
	<string>/dev/console</string>
</dict>
</plist>
EOF
    cat > $staging/System/Library/LaunchDaemons/sh.kido.hostname.plist <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>Label</key>
	<string>sh.kido.hostname</string>
	<key>ProgramArguments</key>
	<array>
		<string>/usr/libexec/kido-set-hostname</string>
	</array>
	<key>RunAtLoad</key>
	<true/>
	<key>StandardOutPath</key>
	<string>/dev/console</string>
	<key>StandardErrorPath</key>
	<string>/dev/console</string>
</dict>
</plist>
EOF

    # the kernel log saver comes from the userland build, /etc/no-dmesg-save or pdlogsave=0 turn it off
    cat > $staging/System/Library/LaunchDaemons/sh.kido.dmesg-save.plist <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>Label</key>
	<string>sh.kido.dmesg-save</string>
	<key>ProgramArguments</key>
	<array>
		<string>/usr/libexec/kido-dmesg-save</string>
	</array>
	<key>RunAtLoad</key>
	<true/>
</dict>
</plist>
EOF

    chmod 755 $staging/usr/libexec/kido-set-hostname
    cat > $staging/var/root/.profile <<'EOF'
test -r /etc/profile && . /etc/profile
EOF
    cat > $staging/var/root/.zprofile <<'EOF'
test -r /etc/zprofile && . /etc/zprofile
EOF
    cat > $staging/var/root/.zshrc <<'EOF'
test -r /etc/zshrc && . /etc/zshrc
EOF
    # master.passwd stays root-only, as set where it is written
    chmod 644 \
      $staging/etc/passwd \
      $staging/etc/group \
      $staging/etc/profile \
      $staging/etc/zshenv \
      $staging/etc/zprofile \
      $staging/etc/zshrc \
      $staging/etc/resolv.conf \
      $staging/etc/shells \
      $staging/etc/ttys \
      $staging/var/root/.profile \
      $staging/var/root/.zprofile \
      $staging/var/root/.zshrc

    mkdir -p $staging/etc/ssl
    cp ${cacert}/etc/ssl/certs/ca-bundle.crt $staging/etc/ssl/cert.pem
    chmod 644 $staging/etc/ssl/cert.pem

    # the Debian path, where ports that skip cert.pem look for the roots
    mkdir -p $staging/etc/ssl/certs
    ln -sf ../cert.pem $staging/etc/ssl/certs/ca-certificates.crt

    mkdir -p $staging/System/Library/CoreServices
    cat > $staging/System/Library/CoreServices/SystemVersion.plist <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>ProductBuildVersion</key>
	<string>25F74</string>
	<key>ProductCopyright</key>
	<string>2026 The kido authors</string>
	<key>ProductName</key>
	<string>KidoDarwin</string>
	<key>ProductUserVisibleVersion</key>
	<string>26.5</string>
	<key>ProductVersion</key>
	<string>26.5</string>
</dict>
</plist>
EOF
    chmod 644 $staging/System/Library/CoreServices/SystemVersion.plist

    for applet in \
      awk \
      chmod \
      clear \
      cmp \
      cp \
      date \
      dd \
      df \
      diff \
      du \
      env \
      expr \
      find \
      grep \
      gunzip \
      gzip \
      id \
      ln \
      ls \
      mkdir \
      mknod \
      more \
      mv \
      netcat \
      patch \
      printf \
      readlink \
      realpath \
      reset \
      rm \
      rmdir \
      sed \
      sort \
      stat \
      tar \
      touch \
      truncate \
      tty \
      vi \
      which \
      whoami \
      xargs
    do
      ln -s toybox "$staging/bin/$applet"
    done
    # the rest of the configured toybox applets, only where no real command of that name is installed
    for applet in \
      basename cat cksum cut dirname echo false head hostname kill md5sum pwd seq sha1sum sha256sum sha512sum \
      sleep tail test tr true uname uniq wc yes
    do
      [ -e "$staging/bin/toybox" ] || break
      if [ ! -e "$staging/bin/$applet" ] && [ ! -e "$staging/usr/bin/$applet" ] &&
         [ ! -e "$staging/sbin/$applet" ] && [ ! -e "$staging/usr/sbin/$applet" ]; then
        ln -s toybox "$staging/bin/$applet"
      fi
    done
    ln -s toybox "$staging/usr/bin/env"
    # /bin/sh is the userland launcher, it runs the shell /private/var/select/sh names, bash as on macOS
    mkdir -p "$staging/private/var/select"
    if [ -x "$staging/bin/bash" ]; then
      ln -sfn /bin/bash "$staging/private/var/select/sh"
    else
      ln -sfn /bin/zsh "$staging/private/var/select/sh"
    fi
    [ -e "$staging/bin/sh" ] || ln -sf zsh "$staging/bin/sh"

    # Scripts widely assume `#!/usr/bin/env foo` works, not just `/bin/foo`.
    ln -sf ../../bin/env "$staging/usr/bin/env"

    chmod 1777 \
      "$staging/private/tmp" \
      "$staging/private/var/tmp"

    chmod 700  $staging/var/root
    chmod 755  $staging/var/root/.cache
    chmod 755  $staging/var/empty
    runHook postInstall
  '';
}
