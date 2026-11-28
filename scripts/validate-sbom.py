#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# Validate KidoDarwin's in-image versions.json and standalone SPDX 2.3 JSON SBOM
# (sbom.spdx.json), including SPDX 2.3 schema constraints, primaryPackagePurpose
# and relationshipType enums, PURL syntax, SPDX license expression grammar,
# extracted LicenseRef completeness, and bidirectional cross-checking.

import json
import re
import sys
from pathlib import Path

SPDX_ID_RE = re.compile(r"^SPDXRef-[A-Za-z0-9.-]+$")
LIC_REF_RE = re.compile(r"^LicenseRef-[A-Za-z0-9.-]+$")
RFC3339_UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
LIC_TOKEN_RE = re.compile(r"^[A-Za-z0-9.+-]+$")
PURL_RE = re.compile(r"^pkg:[a-z][a-z0-9.+-]*/[^\s+#?]+@[^\s+#?]+$")

# SPDX 2.3 Clause 7.24 allowed primaryPackagePurpose values.
SPDX23_PURPOSES = {
    "APPLICATION",
    "FRAMEWORK",
    "LIBRARY",
    "CONTAINER",
    "OPERATING-SYSTEM",
    "DEVICE",
    "FIRMWARE",
    "SOURCE",
    "ARCHIVE",
    "FILE",
    "INSTALL",
    "OTHER",
}

# SPDX 2.3 Clause 11.1 relationship types used in KidoDarwin SBOMs.
SPDX23_REL_TYPES = {
    "DESCRIBES",
    "DESCRIBED_BY",
    "CONTAINS",
    "CONTAINED_BY",
    "DEPENDS_ON",
    "DEPENDENCY_OF",
    "BUILD_DEPENDENCY_OF",
    "OPTIONAL_DEPENDENCY_OF",
    "RUNTIME_DEPENDENCY_OF",
    "STATIC_LINK",
    "DYNAMIC_LINK",
}

# Official SPDX 3.25 License List identifiers and exceptions used in KidoDarwin
# and its vendored Rust crates.
KNOWN_SPDX_BASE = {
    "0BSD",
    "APSL-1.0",
    "APSL-1.1",
    "APSL-1.2",
    "APSL-2.0",
    "Apache-2.0",
    "BSD-1-Clause",
    "BSD-2-Clause",
    "BSD-2-Clause-FreeBSD",
    "BSD-2-Clause-Patent",
    "BSD-3-Clause",
    "BSD-4-Clause",
    "BSD-4-Clause-UC",
    "Brian-Gladman-3-Clause",
    "CC0-1.0",
    "CDDL-1.0",
    "CMU-Mach",
    "GPL-1.0-or-later",
    "GPL-2.0",
    "GPL-2.0-only",
    "GPL-2.0-or-later",
    "GPL-3.0-only",
    "GPL-3.0-or-later",
    "HPND",
    "ISC",
    "LGPL-2.1-only",
    "LGPL-2.1-or-later",
    "LGPL-3.0-only",
    "LGPL-3.0-or-later",
    "MIT",
    "MIT-0",
    "MPL-1.1",
    "MPL-2.0",
    "NCSA",
    "OpenSSL",
    "PSF-2.0",
    "SSLeay-standalone",
    "Unicode-3.0",
    "Unicode-DFS-2016",
    "Unlicense",
    "X11-distribute-modifications-variant",
    "Zlib",
    "blessing",
    "bzip2-1.0.6",
    "curl",
    "dtoa",
}

KNOWN_SPDX_EXCEPTIONS = {
    "Bison-exception-2.2",
    "LLVM-exception",
    "Linux-syscall-note",
    "Swift-exception",
}


def validate_spdx_expression(
    expr: str, extracted_refs: set[str], allow_noassertion: bool = False
) -> tuple[list[str], set[str]]:
    errors: list[str] = []
    used_refs: set[str] = set()
    if not isinstance(expr, str) or not expr.strip():
        return ["empty license expression"], used_refs
    if expr in ("NOASSERTION", "NONE"):
        if allow_noassertion:
            return [], used_refs
        return [f"unexpected {expr} where concrete license expression required"], used_refs

    spaced = expr.replace("(", " ( ").replace(")", " ) ")
    toks = spaced.split()
    depth = 0
    state = "term"  # term | op | after_with | after_exc
    lic_count = 0

    for tok in toks:
        if tok == "(":
            if state != "term":
                errors.append(f"unexpected '(' in {expr!r}")
                break
            depth += 1
        elif tok == ")":
            if state not in ("op", "after_exc"):
                errors.append(f"unexpected ')' in {expr!r}")
                break
            depth -= 1
            if depth < 0:
                errors.append(f"unmatched ')' in {expr!r}")
                break
            state = "after_exc"
        elif tok in ("AND", "OR"):
            if state not in ("op", "after_exc"):
                errors.append(f"unexpected operator {tok!r} in {expr!r}")
                break
            state = "term"
        elif tok == "WITH":
            if state != "op":
                errors.append(f"unexpected WITH in {expr!r}")
                break
            state = "after_with"
        elif state == "after_with":
            if tok not in KNOWN_SPDX_EXCEPTIONS:
                errors.append(f"unknown SPDX exception {tok!r} in {expr!r}")
            state = "after_exc"
        else:
            if state != "term":
                errors.append(f"unexpected token {tok!r} (missing operator) in {expr!r}")
                break
            if tok.startswith("LicenseRef-"):
                if not LIC_REF_RE.match(tok):
                    errors.append(f"invalid LicenseRef grammar {tok!r} in {expr!r}")
                elif tok not in extracted_refs:
                    errors.append(f"unextracted LicenseRef {tok!r} not in hasExtractedLicensingInfos")
                else:
                    used_refs.add(tok)
            elif not LIC_TOKEN_RE.match(tok):
                errors.append(f"invalid SPDX token {tok!r} in {expr!r}")
            elif tok not in KNOWN_SPDX_BASE:
                errors.append(f"unknown SPDX license identifier {tok!r} in {expr!r}")
            lic_count += 1
            state = "op"

    if not errors:
        if depth != 0:
            errors.append(f"unclosed '(' in {expr!r}")
        if state not in ("op", "after_exc") or lic_count == 0:
            errors.append(f"incomplete SPDX expression {expr!r}")
    return errors, used_refs


def validate_versions(versions_path: Path) -> tuple[dict, list[str]]:
    errors: list[str] = []
    data = json.loads(versions_path.read_text(encoding="utf-8"))
    if data.get("schemaVersion") != 1:
        errors.append(f"versions.json: expected schemaVersion=1, got {data.get('schemaVersion')!r}")

    img = data.get("image")
    if not isinstance(img, dict):
        errors.append("versions.json: missing 'image' object")
    else:
        for k in ("productName", "productVersion", "buildVersion", "darwinRevision", "nixpkgsRev", "kidoRev", "created"):
            if not isinstance(img.get(k), str) or not img[k]:
                errors.append(f"versions.json: image.{k} must be a non-empty string")
        if isinstance(img.get("created"), str) and not RFC3339_UTC_RE.match(img["created"]):
            errors.append(f"versions.json: image.created {img['created']!r} is not RFC3339 UTC")

    comps = data.get("components")
    if not isinstance(comps, list) or len(comps) < 20:
        errors.append(
            f"versions.json: expected >= 20 components, got {type(comps).__name__} "
            f"len={len(comps) if isinstance(comps, list) else 0}"
        )
    else:
        seen: set[str] = set()
        for idx, c in enumerate(comps):
            if not isinstance(c, dict):
                errors.append(f"versions.json: components[{idx}] is not an object")
                continue
            for k in ("name", "version", "spdx"):
                if not isinstance(c.get(k), str) or not c[k]:
                    errors.append(f"versions.json: components[{idx}].{k} missing/empty")
            if c.get("version") == "unknown":
                errors.append(f"versions.json: component {c.get('name')!r} has placeholder version 'unknown'")
            name = c.get("name", "")
            if name in seen:
                errors.append(f"versions.json: duplicate component name {name!r}")
            seen.add(name)

    size_bytes = versions_path.stat().st_size
    if size_bytes > 16384:
        errors.append(f"versions.json: unexpectedly large ({size_bytes} bytes > 16384)")

    return data, errors


def validate_sbom(sbom_path: Path, versions_data: dict | None) -> tuple[dict, list[str]]:
    errors: list[str] = []
    doc = json.loads(sbom_path.read_text(encoding="utf-8"))

    # 1. Required top-level SPDX 2.3 fields
    if doc.get("spdxVersion") != "SPDX-2.3":
        errors.append(f"sbom: expected spdxVersion='SPDX-2.3', got {doc.get('spdxVersion')!r}")
    if doc.get("dataLicense") != "CC0-1.0":
        errors.append(f"sbom: expected dataLicense='CC0-1.0', got {doc.get('dataLicense')!r}")
    if doc.get("SPDXID") != "SPDXRef-DOCUMENT":
        errors.append(f"sbom: expected SPDXID='SPDXRef-DOCUMENT', got {doc.get('SPDXID')!r}")
    if not isinstance(doc.get("name"), str) or not doc["name"]:
        errors.append("sbom: missing/empty 'name'")
    if "documentDescribes" in doc:
        errors.append("sbom: deprecated field 'documentDescribes' should be omitted in favor of DESCRIBES relationship")
    ns = doc.get("documentNamespace", "")
    if not isinstance(ns, str) or not ns.startswith("https://") or "#" in ns or ns.endswith("-unknown"):
        errors.append(f"sbom: invalid or non-unique documentNamespace {ns!r}")

    ci = doc.get("creationInfo")
    if not isinstance(ci, dict):
        errors.append("sbom: missing creationInfo")
    else:
        if not isinstance(ci.get("created"), str) or not RFC3339_UTC_RE.match(ci["created"]):
            errors.append(f"sbom: creationInfo.created invalid: {ci.get('created')!r}")
        creators = ci.get("creators")
        if not isinstance(creators, list) or not creators:
            errors.append("sbom: creationInfo.creators must be non-empty list")
        else:
            for cr in creators:
                if not isinstance(cr, str) or not cr.startswith(("Tool: ", "Organization: ", "Person: ")):
                    errors.append(f"sbom: invalid creator {cr!r}")
        if not isinstance(ci.get("licenseListVersion"), str) or not ci["licenseListVersion"]:
            errors.append("sbom: missing creationInfo.licenseListVersion")

    # 2. Extracted licensing infos
    extracted_refs: set[str] = set()
    ext_list = doc.get("hasExtractedLicensingInfos", [])
    if not isinstance(ext_list, list):
        errors.append("sbom: hasExtractedLicensingInfos must be a list")
    else:
        for entry in ext_list:
            lid = entry.get("licenseId", "")
            if not isinstance(lid, str) or not LIC_REF_RE.match(lid):
                errors.append(f"sbom: invalid extracted licenseId {lid!r}")
            elif lid in extracted_refs:
                errors.append(f"sbom: duplicate extracted licenseId {lid!r}")
            else:
                extracted_refs.add(lid)
            if not isinstance(entry.get("extractedText"), str) or not entry["extractedText"].strip():
                errors.append(f"sbom: empty extractedText for {lid!r}")

    # 3. Packages
    packages = doc.get("packages")
    if not isinstance(packages, list) or len(packages) < 20:
        errors.append("sbom: packages must be a list with >= 20 entries")
        return doc, errors

    pkg_ids: set[str] = {"SPDXRef-DOCUMENT"}
    pkg_by_name: dict[str, dict] = {}
    used_license_refs: set[str] = set()

    for idx, pkg in enumerate(packages):
        sid = pkg.get("SPDXID", "")
        if not isinstance(sid, str) or not SPDX_ID_RE.match(sid):
            errors.append(f"sbom: packages[{idx}] invalid SPDXID {sid!r}")
        elif sid in pkg_ids:
            errors.append(f"sbom: duplicate SPDXID {sid!r}")
        else:
            pkg_ids.add(sid)

        for req in ("name", "versionInfo", "downloadLocation", "licenseConcluded", "licenseDeclared", "copyrightText", "supplier"):
            if not isinstance(pkg.get(req), str) or not pkg[req]:
                errors.append(f"sbom: package {sid or idx} missing/empty {req!r}")
        if pkg.get("versionInfo") == "unknown":
            errors.append(f"sbom: package {sid} has placeholder versionInfo 'unknown'")
        if pkg.get("filesAnalyzed") is not False:
            errors.append(f"sbom: package {sid} must set filesAnalyzed=false when no packageVerificationCode is present")

        purpose = pkg.get("primaryPackagePurpose")
        if purpose not in SPDX23_PURPOSES:
            errors.append(f"sbom: package {sid} invalid primaryPackagePurpose {purpose!r}")

        sup = pkg.get("supplier", "")
        if sup != "NOASSERTION" and not sup.startswith(("Organization: ", "Person: ")):
            errors.append(f"sbom: package {sid} invalid supplier {sup!r}")

        dl = pkg.get("downloadLocation", "")
        if dl != "NOASSERTION" and not dl.startswith(("https://", "http://")):
            errors.append(f"sbom: package {sid} invalid downloadLocation {dl!r}")

        for ref in pkg.get("externalRefs", []):
            if ref.get("referenceType") == "purl":
                loc = ref.get("referenceLocator", "")
                if not isinstance(loc, str) or not PURL_RE.match(loc):
                    errors.append(f"sbom: package {sid} invalid PURL {loc!r}")

        is_root = sid == "SPDXRef-KidoDarwin-Image"
        for field in ("licenseConcluded", "licenseDeclared"):
            val = pkg.get(field, "")
            lerrs, refs = validate_spdx_expression(val, extracted_refs, allow_noassertion=is_root)
            for err in lerrs:
                errors.append(f"sbom: package {sid} {field}: {err}")
            used_license_refs |= refs

        if sid.startswith("SPDXRef-pkg-"):
            pkg_by_name[pkg["name"]] = pkg

    for unused in sorted(extracted_refs - used_license_refs):
        errors.append(f"sbom: unused extracted LicenseRef {unused!r} in hasExtractedLicensingInfos")

    # 4. Relationships
    rels = doc.get("relationships")
    if not isinstance(rels, list) or not rels:
        errors.append("sbom: missing/empty relationships")
    else:
        described: set[str] = set()
        linked: set[str] = set()
        seen_rels: set[tuple[str, str, str]] = set()
        for idx, rel in enumerate(rels):
            a = rel.get("spdxElementId")
            t = rel.get("relationshipType")
            b = rel.get("relatedSpdxElement")
            if a not in pkg_ids:
                errors.append(f"sbom: relationships[{idx}] unknown spdxElementId {a!r}")
            if b not in pkg_ids:
                errors.append(f"sbom: relationships[{idx}] unknown relatedSpdxElement {b!r}")
            if t not in SPDX23_REL_TYPES:
                errors.append(f"sbom: relationships[{idx}] invalid relationshipType {t!r}")
            key = (str(a), str(t), str(b))
            if key in seen_rels:
                errors.append(f"sbom: duplicate relationship {key!r}")
            seen_rels.add(key)
            if a == "SPDXRef-DOCUMENT" and t == "DESCRIBES":
                described.add(b)
            if t in ("CONTAINS", "STATIC_LINK", "DYNAMIC_LINK"):
                linked.add(b)
            if t in ("BUILD_DEPENDENCY_OF", "OPTIONAL_DEPENDENCY_OF", "DEPENDENCY_OF", "RUNTIME_DEPENDENCY_OF"):
                linked.add(a)

        if "SPDXRef-KidoDarwin-Image" not in described:
            errors.append("sbom: missing SPDXRef-DOCUMENT DESCRIBES SPDXRef-KidoDarwin-Image")
        for sid in sorted(pkg_ids - {"SPDXRef-DOCUMENT", "SPDXRef-KidoDarwin-Image"}):
            if sid not in linked:
                errors.append(f"sbom: orphan package {sid} has no containment or dependency relationship")

    # 5. Bidirectional cross-check between versions.json and top-level SPDXRef-pkg-* packages
    if versions_data is not None:
        versions_names: set[str] = set()
        for comp in versions_data.get("components", []):
            cname = comp["name"]
            versions_names.add(cname)
            if cname not in pkg_by_name:
                errors.append(f"cross-check: versions.json component {cname!r} missing from sbom.spdx.json")
                continue
            sp = pkg_by_name[cname]
            if sp["versionInfo"] != comp["version"]:
                errors.append(
                    f"cross-check: {cname} version mismatch versions.json={comp['version']!r} vs sbom={sp['versionInfo']!r}"
                )
            if sp["licenseDeclared"] != comp["spdx"]:
                errors.append(
                    f"cross-check: {cname} license mismatch versions.json={comp['spdx']!r} vs sbom={sp['licenseDeclared']!r}"
                )
            lerrs, _ = validate_spdx_expression(comp["spdx"], extracted_refs)
            for err in lerrs:
                errors.append(f"versions.json: component {cname} spdx: {err}")
        for extra in sorted(set(pkg_by_name.keys()) - versions_names):
            errors.append(f"cross-check: sbom.spdx.json top-level package {extra!r} missing from versions.json")

    return doc, errors


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} VERSIONS_JSON SBOM_SPDX_JSON", file=sys.stderr)
        return 2
    versions_path = Path(sys.argv[1])
    sbom_path = Path(sys.argv[2])

    versions_data, v_errors = validate_versions(versions_path)
    sbom_data, s_errors = validate_sbom(sbom_path, versions_data)

    all_errors = v_errors + s_errors
    if all_errors:
        for err in all_errors:
            print(f"VALIDATION ERROR: {err}", file=sys.stderr)
        return 1

    cargo_count = sum(1 for p in sbom_data["packages"] if p["SPDXID"].startswith("SPDXRef-cargo-"))
    top_count = sum(1 for p in sbom_data["packages"] if p["SPDXID"].startswith("SPDXRef-pkg-"))
    print(
        f"SBOM VALIDATION PASS: versions.json={versions_path.stat().st_size}B "
        f"({len(versions_data['components'])} components), "
        f"sbom.spdx.json={sbom_path.stat().st_size}B "
        f"({len(sbom_data['packages'])} total packages: 1 image + {top_count} base components + "
        f"{cargo_count} kido-agent Cargo crates, "
        f"{len(sbom_data.get('hasExtractedLicensingInfos', []))} extracted LicenseRefs, "
        f"{len(sbom_data['relationships'])} relationships)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
