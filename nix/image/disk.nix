{ stdenv
, lib
, rootfs
, kc
, xnuLoader
, gptfdisk
, util-linux
, dosfstools
, mtools
, e2fsprogs
, fakeroot
, espMB ? 64
, rootMB ? 6144
, efiBinary ? "BOOTAA64.EFI"
# the ext4 root's filesystem and partition uuid, which the host passes as ext4-uuid= to find the root
, rootUUID
# fixed values for what mkfs and sgdisk would otherwise pick at random, so the disk is reproducible
, diskGUID
, espGUID
, espVolumeId
, hashSeed
, bootArgs
}:

stdenv.mkDerivation {
  pname = "kido-disk";
  version = "0.1";

  dontUnpack = true;

  nativeBuildInputs = [ gptfdisk util-linux dosfstools mtools e2fsprogs fakeroot ];

  buildPhase = ''
    runHook preBuild
    export MTOOLS_SKIP_CHECK=1
    LINUX_FS_GUID=0FC63DAF-8483-4772-8E79-3D69D8477DE4

    img=disk.img
    esp_sectors=$((${toString espMB} * 2048))
    root_sectors=$((${toString rootMB} * 2048))
    img_sectors=$((2048 + esp_sectors + root_sectors + 2048))
    truncate -s $((img_sectors * 512)) $img
    sgdisk -U ${diskGUID} \
      -n 1:2048:+${toString espMB}M -t 1:EF00            -c 1:"EFI System Partition" -u 1:${espGUID} \
      -n 2:0:+${toString rootMB}M   -t 2:$LINUX_FS_GUID  -c 2:"Darwin ext4 Root"     -u 2:${rootUUID} \
      $img >/dev/null

    read -r root_start root_size <<<"$(sfdisk -d $img | grep -i type=$LINUX_FS_GUID \
      | sed -E 's/.*start= *([0-9]+), *size= *([0-9]+),.*/\1 \2/')"
    read -r esp_start esp_size <<<"$(sfdisk -d $img | grep -i type=C12A7328 \
      | sed -E 's/.*start= *([0-9]+), *size= *([0-9]+),.*/\1 \2/')"

    truncate -s $((esp_size * 512)) esp.img
    mkfs.vfat -F 32 -n EFI -i ${espVolumeId} --invariant esp.img >/dev/null
    mmd -i esp.img ::/EFI ::/EFI/BOOT
    mcopy -o -i esp.img ${xnuLoader}/img/EFI/BOOT/${efiBinary} ::/EFI/BOOT/${efiBinary}
    mcopy -o -i esp.img ${kc}/kernel                          ::/EFI/BOOT/kernel
    printf '%s' ${lib.escapeShellArg bootArgs} > boot-args.txt
    mcopy -o -i esp.img boot-args.txt                          ::/EFI/BOOT/boot-args.txt

    staging="$PWD/root-staging"
    cp -a ${rootfs} "$staging"
    # the store strips write bits and special modes: u+w gives back the staged 644 and 755, the rest is set again
    chmod -R u+w "$staging"
    chmod 444  "$staging/etc/services" "$staging/etc/protocols"
    chmod 600  "$staging/etc/master.passwd"
    chmod 1777 "$staging/private/tmp" "$staging/private/var/tmp"
    chmod 700  "$staging/var/root"

    dd if=esp.img of=$img bs=512 seek=$esp_start count=$esp_size conv=notrunc status=none

    truncate -s $((root_size * 512)) root.img
    # mke2fs -d records each source file's uid/gid, and under Nix the whole
    # staging tree is owned by the unprivileged build user (uid 1000/gid 100),
    # not root. That breaks anything that checks for root ownership. This fixes that issue.
    fakeroot bash <<FAKESCRIPT
    chown -R 0:0 "$staging"
    mke2fs -q -F -t ext4 \
      -b 4096 \
      -O ^orphan_file \
      -L darwin-ext4 -U ${rootUUID} -E hash_seed=${hashSeed} \
      -d "$staging" \
      root.img >/dev/null
FAKESCRIPT
    # /var/empty must be 0755 (mke2fs -d may have staged it u+rwX-only).
    cat > root-debugfs.cmds <<'EOF'
set_inode_field /var/empty mode 040755
EOF
    debugfs -w -f root-debugfs.cmds root.img >/dev/null

    dd if=root.img of=$img bs=512 seek=$root_start count=$root_size conv=notrunc,sparse status=none
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out
    cp --sparse=always disk.img $out/disk.img
    runHook postInstall
  '';

  meta = with lib; {
    description = "Bootable GPT disk image: xnu-loader and the kernel collection on the ESP, an ext4 root";
    platforms = platforms.unix;
  };
}
