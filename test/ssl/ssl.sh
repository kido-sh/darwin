#!/bin/sh
# Copyright (c) 2026 The kido authors
# SPDX-License-Identifier: Apache-2.0

set -u
work=$(cd "$(dirname "$0")" && pwd)
openssl=${OPENSSL:-/usr/bin/openssl}
ocspcheck=${OCSPCHECK:-/usr/bin/ocspcheck}
pass=0
fail=0
pids=

expect() {
    name=$1 want=$2 text=$3
    shift 3
    out=$(: | "$@" 2>&1)
    rc=$?
    if [ "$want" = fail ]; then [ $rc -ne 0 ]; else [ $rc -eq "$want" ]; fi && case $out in *"$text"*) true ;; *) false ;; esac
    if [ $? -eq 0 ]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        printf 'FAIL %s (rc=%s): %s\n' "$name" "$rc" "$(printf '%s' "$out" | tail -n 4 | tr '\n' ' ')"
    fi
}

sha() {
    "$@" 2>/dev/null | "$openssl" dgst -sha256
}

serve() {
    "$openssl" s_server -quiet "$@" </dev/null >/dev/null 2>&1 &
    pids="$pids $!"
}

ready() {
    i=0
    while [ $i -lt 50 ]; do
        : | "$openssl" s_client -connect "127.0.0.1:$1" >/dev/null 2>&1 && return 0
        sleep 0.2
        i=$((i + 1))
    done
    return 1
}

newkey() {
    "$openssl" ecparam -name prime256v1 -genkey -noout -out "$1"
}

expect "version" 0 "LibreSSL 3.3.6" "$openssl" version
expect "openssldir" 0 'OPENSSLDIR: "/private/etc/ssl"' "$openssl" version -d
expect "openssl.cnf as macOS" 0 "= 025f5e31cd7b2248a0a661d8d346452c31d98e280f3420c3f6017c74c829ae73" \
    "$openssl" dgst -sha256 /etc/ssl/openssl.cnf
expect "x509v3.cnf as macOS" 0 "= 6e72e6f9366198e19491ccc785a0cdfb5a05510609c03fcf388b02aac43fec0b" \
    "$openssl" dgst -sha256 /private/etc/ssl/x509v3.cnf
expect "all ciphers as macOS" 0 0f076d600082c7e82a37ef2e39e5312cf501a699f56d0031489692c04506a034 \
    sha "$openssl" ciphers ALL:COMPLEMENTOFALL
expect "default ciphers as macOS" 0 be704aedb6315b23bf2078073f140298114f0a4462750994e9bab9cad6b6fae0 \
    sha "$openssl" ciphers
expect "cipher commands as macOS" 0 6d625416279efdaada645b0b6ff4fff6895254e75593c51bed294c4860307f48 \
    sha "$openssl" list-cipher-commands
expect "cipher algorithms as macOS" 0 7d8b6b4cc5334e0b005bd8d8e20cf49b8bb621ce6811d6199c387a655f2949e8 \
    sha "$openssl" list-cipher-algorithms
expect "digest commands as macOS" 0 225763e175dae19567ebbefbf98c69574e8732d720b64d856c81ef4281b99edd \
    sha "$openssl" list-message-digest-commands
expect "public key algorithms as macOS" 0 e05cdff439f815834e58498a9f16f74348d0039f3566f4957123c4d8d9588d68 \
    sha "$openssl" list-public-key-algorithms

for arch in arm64e arm64; do
    "$work/t_crypto-$arch" || { rc=$?; fail=$((fail + 1)); echo "FAIL t_crypto-$arch exited $rc"; }
done
expect "no OpenSSL in the image" 0 "none" sh -c 'f=$(find /bin /sbin /usr /System -type f -exec \
    grep -laE "OpenSSL 3[.][0-9]|OSSL_PARAM_BLD|EVP_PKEY_fromdata" {} +); echo "${f:-none}"'
expect "only LibreSSL's libcrypto and libssl" 0 "libcrypto.46.dylib libssl.48.dylib" \
    sh -c 'cd /usr/lib && echo libcrypto* libssl*'

rm -rf "$work/pki"
mkdir -p "$work/pki" && cd "$work/pki" || exit 1
cat > ext.cnf <<'EOF'
[req]
distinguished_name = dn
[dn]
[ca]
basicConstraints = critical,CA:true
keyUsage = critical,keyCertSign,cRLSign
subjectKeyIdentifier = hash
[full]
crlDistributionPoints = dpfull
[dpfull]
fullname = URI:http://127.0.0.1/ca.crl
[relative]
crlDistributionPoints = dprel
[dprel]
relativename = dprelname
[dprelname]
CN = kido
EOF
cat > ca.cnf <<'EOF'
[ca]
default_ca = test
[test]
dir = .
database = index.txt
new_certs_dir = .
certificate = ca.pem
private_key = ca.key
serial = serial
default_md = sha256
policy = any
unique_subject = no
x509_extensions = server
[any]
commonName = supplied
[server]
basicConstraints = CA:false
keyUsage = critical,digitalSignature
extendedKeyUsage = serverAuth
subjectAltName = DNS:localhost,IP:127.0.0.1
authorityInfoAccess = OCSP;URI:http://127.0.0.1:8445
[selfsigned]
basicConstraints = CA:false
keyUsage = critical,digitalSignature
extendedKeyUsage = serverAuth
subjectAltName = DNS:localhost,IP:127.0.0.1
authorityInfoAccess = OCSP;URI:http://127.0.0.1:8447
EOF
: > index.txt
echo 01 > serial
newkey ca.key
"$openssl" req -new -x509 -key ca.key -subj /CN=kido-test-ca -days 3650 -sha256 -config ext.cnf \
    -extensions ca -out ca.pem
newkey other.key
"$openssl" req -new -x509 -key other.key -subj /CN=kido-other-ca -days 3650 -sha256 -config ext.cnf \
    -extensions ca -out other.pem
for c in server expired revoked selfsigned; do
    newkey $c.key
    "$openssl" req -new -key $c.key -subj /CN=localhost -out $c.csr
done
"$openssl" ca -batch -notext -config ca.cnf -days 30 -in server.csr -out server.pem 2>/dev/null
"$openssl" ca -batch -notext -config ca.cnf -startdate 200101000000Z -enddate 200201000000Z \
    -in expired.csr -out expired.pem 2>/dev/null
"$openssl" ca -batch -notext -config ca.cnf -days 30 -in revoked.csr -out revoked.pem 2>/dev/null
"$openssl" ca -config ca.cnf -revoke revoked.pem 2>/dev/null
"$openssl" ca -batch -notext -config ca.cnf -days 30 -extensions selfsigned -in selfsigned.csr \
    -out selfsigned.pem 2>/dev/null
for c in server revoked selfsigned; do
    cat $c.pem ca.pem > $c-chain.pem
done
expect "certificates issued" 0 "" test -s server.pem -a -s expired.pem -a -s revoked.pem -a -s selfsigned.pem

printf 'HTTP/1.0 200 ok\r\nContent-Length: 3\r\n\r\nok\n' > ok.txt
serve -accept 8443 -cert server.pem -key server.key -HTTP
serve -accept 8444 -cert expired.pem -key expired.key -HTTP
"$openssl" ocsp -index index.txt -port 8445 -rsigner ca.pem -rkey ca.key -CA ca.pem -ndays 1 \
    -ignore_err >/dev/null 2>&1 &
pids="$pids $!"
"$openssl" ocsp -index index.txt -port 8447 -rsigner selfsigned.pem -rkey selfsigned.key -CA ca.pem \
    -ndays 1 -resp_no_certs -ignore_err >/dev/null 2>&1 &
pids="$pids $!"
sleep 60 | "$openssl" s_server -quiet -dtls1_2 -accept 8446 -cert server.pem -key server.key >/dev/null 2>&1 &
pids="$pids $!"
ready 8443 && ready 8444
expect "servers up" 0 "" true
sleep 1

expect "TLS 1.3" 0 "Protocol  : TLSv1.3" \
    "$openssl" s_client -connect 127.0.0.1:8443 -CAfile ca.pem -verify 5 -verify_return_error -tls1_3
expect "TLS 1.3 verified" 0 "Verify return code: 0 (ok)" \
    "$openssl" s_client -connect 127.0.0.1:8443 -CAfile ca.pem -verify 5 -verify_return_error -tls1_3
expect "TLS 1.2" 0 "Protocol  : TLSv1.2" \
    "$openssl" s_client -connect 127.0.0.1:8443 -CAfile ca.pem -verify 5 -verify_return_error -tls1_2
expect "TLS 1.2 cipher" 0 "Cipher    : ECDHE-ECDSA-" \
    "$openssl" s_client -connect 127.0.0.1:8443 -CAfile ca.pem -verify 5 -verify_return_error -tls1_2
expect "untrusted refused" fail "verify error:num=20:unable to get local issuer certificate" \
    "$openssl" s_client -connect 127.0.0.1:8443 -CAfile other.pem -verify 5 -verify_return_error
expect "expired refused" fail "verify error:num=10:certificate has expired" \
    "$openssl" s_client -connect 127.0.0.1:8444 -CAfile ca.pem -verify 5 -verify_return_error
expect "DTLS 1.2" 0 "Protocol  : DTLSv1.2" \
    "$openssl" s_client -dtls1_2 -connect 127.0.0.1:8446 -CAfile ca.pem -verify 5 -verify_return_error

for arch in arm64e arm64; do
    "$work/t_tls-$arch" "$work/pki" 8443 8444 || { rc=$?; fail=$((fail + 1)); echo "FAIL t_tls-$arch exited $rc"; }
done
out=$("$work/t_libcurl" "$work/pki" 8443 8444) || { rc=$?; fail=$((fail + 1)); echo "FAIL t_libcurl exited $rc"; }
printf '%s\n' "$out" | grep -v '^result'
"$openssl" req -new -x509 -newkey rsa:2048 -nodes -keyout rsakex.key -subj /CN=kido-rsakex -days 30 -out rsakex.pem \
    2>/dev/null
for arch in arm64e arm64; do
    "$work/t_rsakex-$arch" rsakex.key rsakex.pem || { rc=$?; fail=$((fail + 1)); echo "FAIL t_rsakex-$arch exited $rc"; }
done

expect "OCSP good" 0 "" "$ocspcheck" -C ca.pem server-chain.pem
expect "OCSP revoked" fail "certificate is revoked" "$ocspcheck" -C ca.pem revoked-chain.pem
expect "OCSP signed by the leaf refused" fail "OCSP verify failed" \
    "$ocspcheck" -C ca.pem selfsigned-chain.pem

expect "CRL point full name" 0 "" "$openssl" x509 -req -in server.csr -CA ca.pem -CAkey ca.key -set_serial 100 \
    -extfile ext.cnf -extensions full -out full.pem
expect "CRL point relative name refused" fail "" "$openssl" x509 -req -in server.csr -CA ca.pem -CAkey ca.key \
    -set_serial 101 -extfile ext.cnf -extensions relative -out relative.pem

cat > nc.cnf <<'EOF'
[req]
distinguished_name = dn
[dn]
[dns]
basicConstraints = critical,CA:true
keyUsage = critical,keyCertSign,cRLSign
nameConstraints = critical,permitted;DNS:example.com
[uri]
basicConstraints = critical,CA:true
keyUsage = critical,keyCertSign,cRLSign
nameConstraints = critical,permitted;URI:ok.example
[inside]
subjectAltName = DNS:www.example.com
[suffix]
subjectAltName = DNS:badexample.com
[email]
subjectAltName = email:kido@example.com
[userinfo]
subjectAltName = URI:http://ok.example:x@evil.example/
EOF
for c in dns uri; do
    "$openssl" req -new -x509 -key ca.key -subj /CN=kido-nc-$c -days 30 -sha256 -config nc.cnf -extensions $c \
        -out nc-$c.pem
done
for c in inside:dns:www.example.com suffix:dns:badexample.com email:dns:evil.example userinfo:uri:ok.example; do
    name=${c%%:*} rest=${c#*:}
    "$openssl" req -new -key server.key -subj "/CN=${rest#*:}" -out nc-$name.csr
    "$openssl" x509 -req -in nc-$name.csr -CA nc-${rest%%:*}.pem -CAkey ca.key -set_serial 7 -days 30 \
        -extfile nc.cnf -extensions $name -out nc-$name.pem 2>/dev/null
done
expect "name constraint met" 0 "nc-inside.pem: OK" "$openssl" verify -CAfile nc-dns.pem nc-inside.pem
expect "name constraint on a label boundary" fail "permitted subtree violation" \
    "$openssl" verify -CAfile nc-dns.pem nc-suffix.pem
expect "name constraint on the subject's CN" fail "permitted subtree violation" \
    "$openssl" verify -CAfile nc-dns.pem nc-email.pem
expect "name constraint on a URI's host after userinfo" fail "permitted subtree violation" \
    "$openssl" verify -CAfile nc-uri.pem nc-userinfo.pem

{ printf '\060\103\002\001\003\060\013\006\011\052\206\110\206\367\015\001\007\001\060\061\060\041'
  printf '\060\011\006\005\053\016\003\002\032\005\000\004\024'
  printf '\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000'
  printf '\004\010\001\001\001\001\001\001\001\001\002\002\010\000'; } > nocontent.p12
expect "PKCS#12 MAC over absent content" fail "Mac verify error" \
    "$openssl" pkcs12 -in nocontent.p12 -passin pass:kido -noout

printf 'kido pwri\n' > msg.txt
expect "CMS password encrypt" 0 "" "$openssl" cms -encrypt -in msg.txt -aes256 -pwri_password secret123 -out msg.cms
expect "CMS password decrypt" 0 "kido pwri" "$openssl" cms -decrypt -in msg.cms -pwri_password secret123
expect "CMS wrong password" fail "" "$openssl" cms -decrypt -in msg.cms -pwri_password wrong

for p in $pids; do
    kill "$p" 2>/dev/null
done
printf 'openssl, ocspcheck and the PKI: %d/%d passed\n' "$pass" $((pass + fail))
[ $fail -eq 0 ]
