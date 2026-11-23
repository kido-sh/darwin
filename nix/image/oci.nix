# The image as an OCI image layout, in the form kido save and kido push write it: an index entry
# named darwin:latest for darwin/arm64, a kido artifact manifest whose config is image.json without
# name, tag and size, and the disk and the initrd compressed with zstd beside the kernel.
{ runCommand
, zstd
, image
, rootUUID
, created
, revision
}:

runCommand "kido-image-oci" { nativeBuildInputs = [ zstd ]; } ''
  mkdir -p $out/blobs/sha256
  blob() {
    local digest
    digest=$(sha256sum "$1" | cut -d' ' -f1)
    cp "$1" "$out/blobs/sha256/$digest"
    echo "\"digest\":\"sha256:$digest\",\"size\":$(stat -c %s "$1")"
  }
  zstd -q -3 -o disk.img.zst ${image}/disk.img
  zstd -q -3 -o initrd.cpio.zst ${image}/initrd.cpio
  printf '{"created":"%s","rootUUID":"%s","config":{}}' ${created} ${rootUUID} > config.json
  layer() {
    echo -n "{\"mediaType\":\"$1\",$(blob "$2"),\"annotations\":{\"org.opencontainers.image.title\":\"$3\"$4}}"
  }
  uncompressed() {
    echo -n ",\"sh.kido.uncompressed.size\":\"$(stat -c %s "$1")\""
  }
  {
    echo -n '{"schemaVersion":2,"mediaType":"application/vnd.oci.image.manifest.v1+json",'
    echo -n '"artifactType":"application/vnd.kido.image.v1",'
    echo -n "\"config\":{\"mediaType\":\"application/vnd.kido.image.config.v1+json\",$(blob config.json)},\"layers\":["
    layer application/vnd.kido.image.disk.v1.raw+zstd disk.img.zst disk.img "$(uncompressed ${image}/disk.img)"
    echo -n ,
    layer application/vnd.kido.image.kernel.v1 ${image}/kernel kernel ""
    echo -n ,
    layer application/vnd.kido.image.initrd.v1+zstd initrd.cpio.zst initrd.cpio "$(uncompressed ${image}/initrd.cpio)"
    echo -n "],\"annotations\":{\"org.opencontainers.image.created\":\"${created}\","
    echo -n "\"org.opencontainers.image.revision\":\"${revision}\","
    echo -n '"org.opencontainers.image.source":"https://github.com/kido-sh/darwin"}}'
  } > manifest.json
  {
    echo -n '{"schemaVersion":2,"mediaType":"application/vnd.oci.image.index.v1+json","manifests":['
    echo -n "{\"mediaType\":\"application/vnd.oci.image.manifest.v1+json\",$(blob manifest.json),"
    echo -n '"artifactType":"application/vnd.kido.image.v1","platform":{"architecture":"arm64","os":"darwin"},'
    echo -n '"annotations":{"io.containerd.image.name":"darwin:latest","org.opencontainers.image.ref.name":"latest"}}]}'
  } > $out/index.json
  echo -n '{"imageLayoutVersion":"1.0.0"}' > $out/oci-layout
''
