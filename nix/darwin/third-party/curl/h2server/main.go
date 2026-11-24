// Copyright (c) 2026 The kido authors
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/sha256"
	"crypto/tls"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/hex"
	"encoding/pem"
	"flag"
	"fmt"
	"io"
	"log"
	"math/big"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"time"
)

func certificates(dir string) tls.Certificate {
	caKey, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		log.Fatal(err)
	}
	now := time.Now()
	ca := &x509.Certificate{
		SerialNumber:          big.NewInt(1),
		Subject:               pkix.Name{CommonName: "t_http2 test CA"},
		NotBefore:             now.Add(-time.Hour),
		NotAfter:              now.Add(24 * time.Hour),
		KeyUsage:              x509.KeyUsageCertSign,
		BasicConstraintsValid: true,
		IsCA:                  true,
	}
	caDER, err := x509.CreateCertificate(rand.Reader, ca, ca, &caKey.PublicKey, caKey)
	if err != nil {
		log.Fatal(err)
	}
	key, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		log.Fatal(err)
	}
	leaf := &x509.Certificate{
		SerialNumber: big.NewInt(2),
		Subject:      pkix.Name{CommonName: "localhost"},
		NotBefore:    now.Add(-time.Hour),
		NotAfter:     now.Add(24 * time.Hour),
		KeyUsage:     x509.KeyUsageDigitalSignature,
		ExtKeyUsage:  []x509.ExtKeyUsage{x509.ExtKeyUsageServerAuth},
		DNSNames:     []string{"localhost"},
		IPAddresses:  []net.IP{net.ParseIP("127.0.0.1")},
	}
	leafDER, err := x509.CreateCertificate(rand.Reader, leaf, ca, &key.PublicKey, caKey)
	if err != nil {
		log.Fatal(err)
	}
	pemCA := pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: caDER})
	if err := os.WriteFile(filepath.Join(dir, "ca.pem.tmp"), pemCA, 0o644); err != nil {
		log.Fatal(err)
	}
	return tls.Certificate{Certificate: [][]byte{leafDER}, PrivateKey: key}
}

func handler() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprintf(w, "proto=%s method=%s path=%s\n", r.Proto, r.Method, r.URL.Path)
	})
	mux.HandleFunc("/big", func(w http.ResponseWriter, r *http.Request) {
		block := make([]byte, 4096)
		for i := range block {
			block[i] = byte(i * 7)
		}
		for i := 0; i < 256; i++ {
			w.Write(block)
		}
	})
	mux.HandleFunc("/upload", func(w http.ResponseWriter, r *http.Request) {
		h := sha256.New()
		n, _ := io.Copy(h, r.Body)
		fmt.Fprintf(w, "received %d sha256 %s\n", n, hex.EncodeToString(h.Sum(nil)))
	})
	mux.HandleFunc("/trailer", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Trailer", "X-Checksum")
		io.WriteString(w, "body before trailer\n")
		w.Header().Set("X-Checksum", "kido")
	})
	return mux
}

func serve(addr string, cert *tls.Certificate, protos func(*http.Protocols)) {
	ln, err := net.Listen("tcp", addr)
	if err != nil {
		log.Fatal(err)
	}
	srv := &http.Server{Handler: handler(), Protocols: new(http.Protocols)}
	protos(srv.Protocols)
	if cert != nil {
		srv.TLSConfig = &tls.Config{Certificates: []tls.Certificate{*cert}}
		go func() { log.Fatal(srv.ServeTLS(ln, "", "")) }()
		return
	}
	go func() { log.Fatal(srv.Serve(ln)) }()
}

func main() {
	dir := flag.String("dir", ".", "where to write ca.pem")
	tlsAddr := flag.String("tls", ":8443", "HTTP/2 and HTTP/1.1 over TLS")
	tls11Addr := flag.String("tls11", ":8444", "HTTP/1.1 only over TLS")
	h2cAddr := flag.String("h2c", ":8080", "HTTP/1.1 and HTTP/2 with prior knowledge, no TLS")
	flag.Parse()
	cert := certificates(*dir)
	serve(*tlsAddr, &cert, func(p *http.Protocols) { p.SetHTTP1(true); p.SetHTTP2(true) })
	serve(*tls11Addr, &cert, func(p *http.Protocols) { p.SetHTTP1(true) })
	serve(*h2cAddr, nil, func(p *http.Protocols) { p.SetHTTP1(true); p.SetUnencryptedHTTP2(true) })
	if err := os.Rename(filepath.Join(*dir, "ca.pem.tmp"), filepath.Join(*dir, "ca.pem")); err != nil {
		log.Fatal(err)
	}
	select {}
}
