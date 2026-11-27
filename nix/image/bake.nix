# The image directory kido imports: the loader as the kernel, an initrd holding only the kernel
# collection (the loader then takes the kernel command line as the boot-args), the disk and image.json.
{ runCommand
, cpio
, kc
, loader
, disk
, rootUUID
# image.json's creation time, RFC 3339 in UTC
, created
}:

runCommand "kido-image" { nativeBuildInputs = [ cpio ]; } ''
  mkdir -p $out initrd/EFI/BOOT
  cp ${loader}/boot/xnu-loader.Image $out/kernel
  cp ${kc}/kernel initrd/EFI/BOOT/kernel
  chmod 755 initrd/EFI initrd/EFI/BOOT
  chmod 644 initrd/EFI/BOOT/kernel
  touch -h -d @$SOURCE_DATE_EPOCH initrd/EFI initrd/EFI/BOOT initrd/EFI/BOOT/kernel
  (cd initrd && find EFI | LC_ALL=C sort | cpio --quiet -o -H newc -R 0:0 --reproducible) > $out/initrd.cpio
  cp --sparse=always ${disk}/disk.img $out/disk.img
  printf '{"name":"darwin","tag":"latest","created":"%s","rootUUID":"%s","size":%s,"parent":"","config":{"labels":{"sh.kido.virtiofs":"1"}}}\n' \
    ${created} ${rootUUID} "$(stat -c %s $out/disk.img)" > $out/image.json
''
