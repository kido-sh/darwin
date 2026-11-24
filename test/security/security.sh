#!/bin/sh
# Copyright (c) 2026 The kido authors
# SPDX-License-Identifier: Apache-2.0

set -u
openssl=/usr/bin/openssl

newkey() {
    case $2 in
    rsa) "$openssl" genrsa -out "$1" 2048 2>/dev/null ;;
    *) "$openssl" ecparam -name prime256v1 -genkey -noout -out "$1" ;;
    esac
}

setup() {
    mkdir -p "$1" && cd "$1" || exit 1
    cat > ext.cnf <<'EOF'
[req]
distinguished_name = dn
[dn]
[ca]
basicConstraints = critical,CA:true
keyUsage = critical,keyCertSign,cRLSign
subjectKeyIdentifier = hash
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
keyUsage = critical,digitalSignature,keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = DNS:localhost,IP:127.0.0.1
[signer]
basicConstraints = CA:false
keyUsage = critical,digitalSignature,nonRepudiation
extendedKeyUsage = emailProtection
subjectKeyIdentifier = hash
EOF
    : > index.txt
    echo 01 > serial
    for ca in ca other; do
        newkey $ca.key ec
        "$openssl" req -new -x509 -key $ca.key -subj /CN=kido-$ca -days 3650 -sha256 -config ext.cnf -extensions ca \
            -out $ca.pem || exit 1
        "$openssl" x509 -in $ca.pem -outform DER -out $ca.der || exit 1
    done
    for c in server:ec expired:ec rsa:rsa ec:ec; do
        newkey ${c%:*}.key ${c#*:}
        "$openssl" req -new -key ${c%:*}.key -subj /CN=kido-${c%:*} -out ${c%:*}.csr || exit 1
    done
    "$openssl" ca -batch -notext -config ca.cnf -days 30 -in server.csr -out server.pem 2>/dev/null
    "$openssl" ca -batch -notext -config ca.cnf -startdate 200101000000Z -enddate 200201000000Z -in expired.csr \
        -out expired.pem 2>/dev/null
    for c in rsa ec; do
        "$openssl" ca -batch -notext -config ca.cnf -days 30 -extensions signer -in $c.csr -out $c.pem 2>/dev/null
        "$openssl" x509 -in $c.pem -outform DER -out $c.der || exit 1
        "$openssl" pkcs12 -export -inkey $c.key -in $c.pem -certfile ca.pem -name "kido $c" -passout pass:kido \
            -out $c.p12 || exit 1
    done
    "$openssl" pkcs12 -export -inkey rsa.key -in rsa.pem -certfile ca.pem -name "kido aes" -keypbe AES-256-CBC \
        -certpbe AES-256-CBC -passout pass:kido -out aes.p12 || exit 1
    "$openssl" pkcs12 -export -nokeys -in rsa.pem -certfile ca.pem -passout pass:kido -out certs.p12 || exit 1
    { printf '\060\103\002\001\003\060\013\006\011\052\206\110\206\367\015\001\007\001\060\061\060\041'
      printf '\060\011\006\005\053\016\003\002\032\005\000\004\024'
      printf '\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000'
      printf '\004\010\001\001\001\001\001\001\001\001\002\002\010\000'; } > nocontent.p12
    printf 'kido cms message\n' > msg.txt
    printf 'kido cms message, changed\n' > changed.txt
    "$openssl" cms -sign -nodetach -binary -in msg.txt -signer rsa.pem -inkey rsa.key -certfile ca.pem -outform DER \
        -out signed.der || exit 1
    "$openssl" cms -sign -binary -in msg.txt -signer ec.pem -inkey ec.key -outform DER -out detached.der || exit 1
    cat rsa.pem ca.pem > chain.pem
    "$openssl" crl2pkcs7 -nocrl -certfile chain.pem -outform DER -out certs.p7b || exit 1
    printf 'HTTP/1.0 200 ok\r\nContent-Length: 3\r\n\r\nok\n' > ok.txt
}

serve() {
    cd "$1" || exit 1
    "$openssl" s_server -quiet -accept 8453 -cert server.pem -key server.key -HTTP </dev/null >/dev/null 2>&1 &
    "$openssl" s_server -quiet -accept 8454 -cert expired.pem -key expired.key -HTTP </dev/null >/dev/null 2>&1 &
    "$openssl" s_server -quiet -accept 8455 -cert server.pem -key server.key -HTTP -tls1_3 </dev/null >/dev/null 2>&1 &
    "$openssl" s_server -quiet -accept 8456 -cert server.pem -key server.key -HTTP -tls1_2 </dev/null >/dev/null 2>&1 &
    wait
}

ready() {
    for port in 8453 8454 8455 8456; do
        i=0
        until : | "$openssl" s_client -connect 127.0.0.1:$port >/dev/null 2>&1; do
            i=$((i + 1))
            [ $i -lt 50 ] || exit 1
            sleep 0.2
        done
    done
}

compare() {
    bin=$1 dir=$2 good=$3 expired=$4 tls13=$5 tls12=$6
    "$bin/t_pkcs12" constants
    "$bin/t_pkcs12" "RSA p12" "$dir/rsa.p12" kido
    "$bin/t_pkcs12" "EC p12" "$dir/ec.p12" kido
    "$bin/t_pkcs12" "AES p12" "$dir/aes.p12" kido
    "$bin/t_pkcs12" "certificates-only p12" "$dir/certs.p12" kido
    "$bin/t_pkcs12" "wrong password" "$dir/rsa.p12" wrong
    "$bin/t_pkcs12" "MAC over absent content" "$dir/nocontent.p12" kido
    "$bin/t_cms" "signed" "$dir/signed.der" "$dir/ca.der"
    "$bin/t_cms" "signed, changed content" "$dir/signed.der" "$dir/ca.der" changed "$dir/msg.txt"
    "$bin/t_cms" "detached" "$dir/detached.der" "$dir/ca.der" detached "$dir/msg.txt"
    "$bin/t_cms" "detached, changed content" "$dir/detached.der" "$dir/ca.der" detached "$dir/changed.txt"
    "$bin/t_cms" "certificates only" "$dir/certs.p7b" "$dir/ca.der"
    "$bin/t_sectls" "SecureTransport" "$dir/ca.der" "$good" localhost
    "$bin/t_sectls" "SecureTransport, TLS 1.2 at most" "$dir/ca.der" "$good" localhost tls12
    "$bin/t_sectls" "SecureTransport, TLS 1.3 at most" "$dir/ca.der" "$good" localhost tls13
    "$bin/t_sectls" "SecureTransport, TLS 1.3 server" "$dir/ca.der" "$tls13" localhost
    "$bin/t_sectls" "SecureTransport, TLS 1.2 server" "$dir/ca.der" "$tls12" localhost
    "$bin/t_sectls" "SecureTransport, expired" "$dir/ca.der" "$expired" localhost
    "$bin/t_sectls" "SecureTransport, untrusted" "$dir/other.der" "$good" localhost
    "$bin/t_sectls" "SecureTransport, wrong name" "$dir/ca.der" "$good" wrong.example
    "$bin/t_sectls" "SecureTransport, system roots" "$dir/ca.der" "$good" localhost system
    "$bin/t_libcurl" "$dir" "$good" "$expired" | grep '^result'
    "$bin/t_certlegacy" RSA "$dir/rsa.der" EC "$dir/ec.der" | grep '^result'
}

check() {
    bin=$1
    fail=0
    "$bin/t_pkcs12" constants || fail=$((fail + 1))
    "$bin/t_seckey" self || fail=$((fail + 1))
    "$bin/t_seckey" check "$bin/vectors.txt" || fail=$((fail + 1))
    "$bin/t_secspi" "$bin/vectors.txt" || fail=$((fail + 1))
    "$bin/t_seckey" gen > "$bin/pki/guest.vec" || fail=$((fail + 1))
    "$bin/t_seckey" check "$bin/pki/guest.vec" || fail=$((fail + 1))
    "$bin/t_certlegacy" RSA "$bin/pki/rsa.der" EC "$bin/pki/ec.der" || fail=$((fail + 1))
    exit $fail
}

case ${1:-} in
setup) setup "$2" ;;
serve) serve "$2" ;;
ready) ready ;;
compare) compare "$2" "$3" "$4" "$5" "$6" "$7" ;;
check) check "$2" ;;
*)
    echo "usage: security.sh setup DIR | serve DIR | ready | compare BIN DIR GOOD EXPIRED TLS13 TLS12 | check BIN" >&2
    exit 2
    ;;
esac
