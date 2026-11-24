/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#define XML_DTD 1
#define XML_GE 1

#include <expat.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"

struct doc {
    const char *name;
    const char *data;
    size_t len;
};

#define DOC(name, text) { name, text, sizeof(text) - 1 }

static const struct doc docs[] = {
    DOC("simple", "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<a x=\"1\" y='2'>text<b/>&amp;&#x41;&#66;<![CDATA[<raw>]]><!--c--><?pi data?>\n</a>"),
    DOC("doctype", "<!DOCTYPE r [<!ENTITY e \"ent\"><!ENTITY u SYSTEM \"u.txt\" NDATA n>"
        "<!ATTLIST r d CDATA \"def\" i ID #IMPLIED><!ELEMENT r (#PCDATA|s)*><!ELEMENT s EMPTY>"
        "<!NOTATION n SYSTEM \"x\">]><r i=\"k\">&e;<s/></r>"),
    DOC("namespaces", "<p:a xmlns:p=\"urn:p\" xmlns=\"urn:d\"><b p:c=\"1\" c=\"2\"/><p:d xmlns:p=\"urn:q\"/></p:a>"),
    DOC("latin1", "<?xml version='1.0' encoding='ISO-8859-1'?><a t='\xe9'>\xe9\xff</a>"),
    DOC("utf16le", "\xff\xfe<\0a\0>\0\xe9\0\x3d\xd8\x00\xde<\0/\0a\0>\0"),
    DOC("external", "<!DOCTYPE a SYSTEM \"ext.dtd\" [<!ENTITY x SYSTEM \"x.xml\">]><a>&x;&skipped;</a>"),
    DOC("mismatch", "<a><b></a>"),
    DOC("junk", "<a/><b/>"),
    DOC("charref", "<a>&#0;</a>"),
    DOC("undefined", "<a>&nope;</a>"),
    DOC("duplicate", "<a x='1' x='2'/>"),
    DOC("badutf8", "<a>\xc3\x28</a>"),
    DOC("truncated", "<a>tex"),
    DOC("empty", ""),
    DOC("nodoc", "  \n "),
    DOC("badxmldecl", "<?xml version='2.0' encoding='nope'?><a/>"),
    DOC("unknownenc", "<?xml version='1.0' encoding='x-kido'?><a>\x01\x02</a>"),
    DOC("prefix", "<a xmlns:xml='urn:wrong'/>"),
    DOC("unbound", "<p:a/>"),
    DOC("laughs", "<!DOCTYPE l [<!ENTITY a \"aaaaaaaaaa\"><!ENTITY b \"&a;&a;&a;&a;&a;&a;&a;&a;&a;&a;\">"
        "<!ENTITY c \"&b;&b;&b;&b;&b;&b;&b;&b;&b;&b;\"><!ENTITY d \"&c;&c;&c;&c;&c;&c;&c;&c;&c;&c;\">"
        "<!ENTITY e \"&d;&d;&d;&d;&d;&d;&d;&d;&d;&d;\"><!ENTITY f \"&e;&e;&e;&e;&e;&e;&e;&e;&e;&e;\">"
        "<!ENTITY g \"&f;&f;&f;&f;&f;&f;&f;&f;&f;&f;\"><!ENTITY h \"&g;&g;&g;&g;&g;&g;&g;&g;&g;&g;\">]>"
        "<l>&h;</l>"),
    DOC("recursive", "<!DOCTYPE r [<!ENTITY a \"&b;\"><!ENTITY b \"&a;\">]><r>&a;</r>"),
    DOC("crlf", "<a\r\n  x='1\r\n2'>l1\r\nl2\rl3</a>\r\n"),
    DOC("whitespace", "<a x=' \t\n v '>\t <b/> \n</a>"),
    DOC("newname", "<\xe2\xb0\x80>new name rules</\xe2\xb0\x80>"),
    DOC("surrogate", "\xff\xfe<\0a\0>\0x\0\x00\xd8y\0<\0/\0a\0>\0"),
};

static FILE *out;
static char where[64];
static char text[65536];
static size_t textlen;

static void
show(const char *s, int len)
{
    for (int i = 0; len < 0 ? s[i] != '\0' : i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x20 || c >= 0x7f || c == '\\') {
            fprintf(out, "\\x%02x", c);
        } else {
            fputc(c, out);
        }
    }
}

static void
flush_text(void)
{
    if (textlen > 0) {
        fprintf(out, "%s text \"", where);
        show(text, (int)textlen);
        fprintf(out, "\"\n");
        textlen = 0;
    }
}

static void
event(const char *fmt, ...)
{
    va_list ap;

    flush_text();
    fprintf(out, "%s ", where);
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
}

static void XMLCALL
on_start(void *u, const XML_Char *name, const XML_Char **atts)
{
    XML_Parser p = u;

    event("start %s at %lu:%lu specified %d id %d", name, (unsigned long)XML_GetCurrentLineNumber(p),
          (unsigned long)XML_GetCurrentColumnNumber(p), XML_GetSpecifiedAttributeCount(p),
          XML_GetIdAttributeIndex(p));
    for (int i = 0; atts[i] != NULL; i += 2) {
        fprintf(out, " %s=\"", atts[i]);
        show(atts[i + 1], -1);
        fprintf(out, "\"");
    }
    fprintf(out, "\n");
}

static void XMLCALL
on_end(void *u, const XML_Char *name)
{
    (void)u;
    event("end %s\n", name);
}

static void XMLCALL
on_text(void *u, const XML_Char *s, int len)
{
    (void)u;
    if (textlen + (size_t)len < sizeof(text)) {
        memcpy(text + textlen, s, (size_t)len);
        textlen += (size_t)len;
    }
}

static void XMLCALL
on_pi(void *u, const XML_Char *target, const XML_Char *data)
{
    (void)u;
    event("pi %s \"%s\"\n", target, data);
}

static void XMLCALL
on_comment(void *u, const XML_Char *data)
{
    (void)u;
    event("comment \"%s\"\n", data);
}

static void XMLCALL
on_cdata_start(void *u)
{
    (void)u;
    event("cdata-start\n");
}

static void XMLCALL
on_cdata_end(void *u)
{
    (void)u;
    event("cdata-end\n");
}

static void XMLCALL
on_xmldecl(void *u, const XML_Char *version, const XML_Char *encoding, int standalone)
{
    (void)u;
    event("xmldecl %s %s %d\n", version ? version : "-", encoding ? encoding : "-", standalone);
}

static void XMLCALL
on_doctype_start(void *u, const XML_Char *name, const XML_Char *sysid, const XML_Char *pubid,
                 int internal)
{
    (void)u;
    event("doctype %s system %s public %s internal %d\n", name, sysid ? sysid : "-", pubid ? pubid : "-",
          internal);
}

static void XMLCALL
on_doctype_end(void *u)
{
    (void)u;
    event("doctype-end\n");
}

static void XMLCALL
on_entity(void *u, const XML_Char *name, int param, const XML_Char *value, int len, const XML_Char *base,
          const XML_Char *sysid, const XML_Char *pubid, const XML_Char *notation)
{
    (void)u;
    (void)base;
    event("entity %s param %d value \"", name, param);
    if (value != NULL) {
        show(value, len);
    }
    fprintf(out, "\" system %s public %s notation %s\n", sysid ? sysid : "-", pubid ? pubid : "-",
            notation ? notation : "-");
}

static void XMLCALL
on_notation(void *u, const XML_Char *name, const XML_Char *base, const XML_Char *sysid,
            const XML_Char *pubid)
{
    (void)u;
    (void)base;
    event("notation %s system %s public %s\n", name, sysid ? sysid : "-", pubid ? pubid : "-");
}

static void XMLCALL
on_attlist(void *u, const XML_Char *el, const XML_Char *att, const XML_Char *type, const XML_Char *dflt,
           int required)
{
    (void)u;
    event("attlist %s %s %s %s %d\n", el, att, type, dflt ? dflt : "-", required);
}

static void
show_model(XML_Content *m)
{
    fprintf(out, "(%d,%d,%s", m->type, m->quant, m->name ? m->name : "-");
    for (unsigned i = 0; i < m->numchildren; i++) {
        show_model(&m->children[i]);
    }
    fprintf(out, ")");
}

static void XMLCALL
on_element_decl(void *u, const XML_Char *name, XML_Content *model)
{
    XML_Parser p = u;

    event("element %s ", name);
    show_model(model);
    fprintf(out, "\n");
    XML_FreeContentModel(p, model);
}

static void XMLCALL
on_ns_start(void *u, const XML_Char *prefix, const XML_Char *uri)
{
    (void)u;
    event("ns-start %s %s\n", prefix ? prefix : "-", uri ? uri : "-");
}

static void XMLCALL
on_ns_end(void *u, const XML_Char *prefix)
{
    (void)u;
    event("ns-end %s\n", prefix ? prefix : "-");
}

static void XMLCALL
on_skipped(void *u, const XML_Char *name, int param)
{
    (void)u;
    event("skipped %s param %d\n", name, param);
}

static int XMLCALL
on_external(XML_Parser p, const XML_Char *context, const XML_Char *base, const XML_Char *sysid,
            const XML_Char *pubid)
{
    (void)base;
    event("external context %s system %s public %s\n", context ? context : "-", sysid ? sysid : "-",
          pubid ? pubid : "-");
    if (context == NULL) {
        return XML_STATUS_OK;
    }
    XML_Parser sub = XML_ExternalEntityParserCreate(p, context, NULL);
    if (sub == NULL) {
        return XML_STATUS_ERROR;
    }
    static const char body[] = "<in>from x</in>";
    enum XML_Status st = XML_Parse(sub, body, sizeof(body) - 1, XML_TRUE);
    XML_ParserFree(sub);
    return st;
}

static int XMLCALL
on_unknown_encoding(void *u, const XML_Char *name, XML_Encoding *info)
{
    (void)u;
    if (strcmp(name, "x-kido") != 0) {
        return XML_STATUS_ERROR;
    }
    for (int i = 0; i < 256; i++) {
        info->map[i] = i < 0x80 ? i : -1;
    }
    info->map[1] = 0x263a;
    info->map[2] = 0xe9;
    info->data = NULL;
    info->convert = NULL;
    info->release = NULL;
    return XML_STATUS_OK;
}

static XML_Parser
make(const struct doc *d)
{
    bool ns = strcmp(d->name, "namespaces") == 0 || strcmp(d->name, "prefix") == 0 ||
              strcmp(d->name, "unbound") == 0;
    XML_Parser p = ns ? XML_ParserCreateNS(NULL, '|') : XML_ParserCreate(NULL);

    if (ns) {
        XML_SetReturnNSTriplet(p, 1);
    }
    XML_SetUserData(p, p);
    XML_SetElementHandler(p, on_start, on_end);
    XML_SetCharacterDataHandler(p, on_text);
    XML_SetProcessingInstructionHandler(p, on_pi);
    XML_SetCommentHandler(p, on_comment);
    XML_SetCdataSectionHandler(p, on_cdata_start, on_cdata_end);
    XML_SetXmlDeclHandler(p, on_xmldecl);
    XML_SetDoctypeDeclHandler(p, on_doctype_start, on_doctype_end);
    XML_SetEntityDeclHandler(p, on_entity);
    XML_SetNotationDeclHandler(p, on_notation);
    XML_SetAttlistDeclHandler(p, on_attlist);
    XML_SetElementDeclHandler(p, on_element_decl);
    XML_SetNamespaceDeclHandler(p, on_ns_start, on_ns_end);
    XML_SetSkippedEntityHandler(p, on_skipped);
    XML_SetExternalEntityRefHandler(p, on_external);
    XML_SetUnknownEncodingHandler(p, on_unknown_encoding, NULL);
    XML_SetParamEntityParsing(p, XML_PARAM_ENTITY_PARSING_UNLESS_STANDALONE);
    return p;
}

static void
finish(XML_Parser p, enum XML_Status st)
{
    flush_text();
    enum XML_Error e = XML_GetErrorCode(p);
    fprintf(out, "%s status %d error %d \"%s\"", where, st, e, XML_ErrorString(e));
    if (st != XML_STATUS_OK) {
        fprintf(out, " at %lu:%lu", (unsigned long)XML_GetCurrentLineNumber(p),
                (unsigned long)XML_GetCurrentColumnNumber(p));
    }
    fprintf(out, "\n");
}

static enum XML_Status
parse_whole(const struct doc *d)
{
    XML_Parser p = make(d);
    enum XML_Status st = XML_Parse(p, d->data, (int)d->len, XML_TRUE);
    finish(p, st);
    XML_ParserFree(p);
    return st;
}

static enum XML_Status
parse_bytes(const struct doc *d)
{
    XML_Parser p = make(d);
    enum XML_Status st = XML_STATUS_OK;

    for (size_t i = 0; i < d->len && st == XML_STATUS_OK; i++) {
        st = XML_Parse(p, d->data + i, 1, XML_FALSE);
    }
    if (st == XML_STATUS_OK) {
        st = XML_Parse(p, NULL, 0, XML_TRUE);
    }
    finish(p, st);
    XML_ParserFree(p);
    return st;
}

static int
expected_ok(const char *name)
{
    static const char *ok[] = { "simple", "doctype", "namespaces", "latin1", "utf16le", "external", "unknownenc",
                                "crlf", "whitespace", "newname", NULL };
    for (int i = 0; ok[i] != NULL; i++) {
        if (strcmp(ok[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static void
check_buffer_api(void)
{
    XML_Parser p = XML_ParserCreate("UTF-8");
    static const char doc[] = "<buf a='1'>chunked</buf>";
    void *buf = XML_GetBuffer(p, 8);
    CHECK(buf != NULL);
    memcpy(buf, doc, 8);
    CHECK(XML_ParseBuffer(p, 8, XML_FALSE) == XML_STATUS_OK);
    buf = XML_GetBuffer(p, (int)sizeof(doc) - 9);
    memcpy(buf, doc + 8, sizeof(doc) - 9);
    CHECK(XML_ParseBuffer(p, (int)sizeof(doc) - 9, XML_TRUE) == XML_STATUS_OK);
    XML_ParsingStatus ps;
    XML_GetParsingStatus(p, &ps);
    CHECK(ps.parsing == XML_FINISHED && ps.finalBuffer == XML_TRUE);
    CHECK(XML_ParserReset(p, NULL) == XML_TRUE);
    CHECK(XML_Parse(p, "<r/>", 4, XML_TRUE) == XML_STATUS_OK);
    XML_ParserFree(p);

    p = XML_ParserCreate(NULL);
    CHECK(XML_SetBillionLaughsAttackProtectionMaximumAmplification(p, 0.5f) == XML_FALSE);
    CHECK(XML_SetBillionLaughsAttackProtectionMaximumAmplification(p, 2.0f) == XML_TRUE);
    CHECK(XML_SetBillionLaughsAttackProtectionActivationThreshold(p, 1024) == XML_TRUE);
    CHECK(XML_SetAllocTrackerMaximumAmplification(p, 0.5f) == XML_FALSE);
    CHECK(XML_SetAllocTrackerActivationThreshold(p, 1 << 20) == XML_TRUE);
    CHECK(XML_SetHashSalt(p, 42) == 1);
    CHECK(XML_SetEncoding(p, "ISO-8859-1") == XML_STATUS_OK);
    CHECK(XML_SetBase(p, "file:///base/") == XML_STATUS_OK && strcmp(XML_GetBase(p), "file:///base/") == 0);
    CHECK(XML_SetReparseDeferralEnabled(p, XML_FALSE) == XML_TRUE);
    CHECK(XML_StopParser(p, XML_FALSE) == XML_STATUS_ERROR && XML_GetErrorCode(p) == XML_ERROR_NOT_STARTED);
    XML_ParserFree(p);

    XML_Memory_Handling_Suite mem = { malloc, realloc, free };
    p = XML_ParserCreate_MM(NULL, &mem, NULL);
    CHECK(p != NULL && XML_Parse(p, "<m/>", 4, XML_TRUE) == XML_STATUS_OK);
    void *block = XML_MemMalloc(p, 16);
    block = XML_MemRealloc(p, block, 32);
    CHECK(block != NULL);
    XML_MemFree(p, block);
    XML_ParserFree(p);
}

static void
check_laughs_limit(void)
{
    const struct doc *d = NULL;
    for (size_t i = 0; i < sizeof(docs) / sizeof(docs[0]); i++) {
        if (strcmp(docs[i].name, "laughs") == 0) {
            d = &docs[i];
        }
    }
    XML_Parser p = XML_ParserCreate(NULL);
    CHECK(XML_Parse(p, d->data, (int)d->len, XML_TRUE) == XML_STATUS_ERROR &&
          XML_GetErrorCode(p) == XML_ERROR_AMPLIFICATION_LIMIT_BREACH);
    XML_ParserFree(p);
    p = XML_ParserCreate(NULL);
    CHECK(XML_SetBillionLaughsAttackProtectionMaximumAmplification(p, 1.0e9f) == XML_TRUE);
    CHECK(XML_SetBillionLaughsAttackProtectionActivationThreshold(p, 1ULL << 40) == XML_TRUE);
    CHECK(XML_Parse(p, d->data, (int)d->len, XML_TRUE) == XML_STATUS_OK);
    XML_ParserFree(p);
}

static void
check_deep(void)
{
    enum { N = 20000 };
    static char deep[N * 7 + 1];
    size_t n = 0;
    for (int i = 0; i < N; i++) {
        memcpy(deep + n, "<d>", 3);
        n += 3;
    }
    for (int i = 0; i < N; i++) {
        memcpy(deep + n, "</d>", 4);
        n += 4;
    }
    XML_Parser p = XML_ParserCreate(NULL);
    CHECK(XML_Parse(p, deep, (int)n, XML_TRUE) == XML_STATUS_OK);
    XML_ParserFree(p);
    fprintf(out, "deep nesting %d: ok\n", N);
}

int
main(int argc, char **argv)
{
    out = stdout;
    const XML_LChar *v = XML_ExpatVersion();
    XML_Expat_Version vi = XML_ExpatVersionInfo();
    printf("version %s (%d.%d.%d)\n", v, vi.major, vi.minor, vi.micro);
    CHECK(strncmp(v, "expat_", 6) == 0 && vi.major == 2);
    for (const XML_Feature *f = XML_GetFeatureList(); f->feature != XML_FEATURE_END; f++) {
        printf("feature %d %s %ld\n", f->feature, f->name, f->value);
    }
    for (int e = XML_ERROR_NONE; e <= XML_ERROR_NOT_STARTED; e++) {
        printf("error %d \"%s\"\n", e, XML_ErrorString((enum XML_Error)e));
    }
    for (size_t i = 0; i < sizeof(docs) / sizeof(docs[0]); i++) {
        const struct doc *d = &docs[i];
        snprintf(where, sizeof(where), "%s whole:", d->name);
        enum XML_Status whole = parse_whole(d);
        snprintf(where, sizeof(where), "%s bytewise:", d->name);
        enum XML_Status bytes = parse_bytes(d);
        if (argc < 2 || strcmp(argv[1], "compare") != 0) {
            CHECK((whole == XML_STATUS_OK) == expected_ok(d->name));
            CHECK(whole == bytes);
        }
    }
    check_buffer_api();
    check_laughs_limit();
    check_deep();
    return checks_done("expat");
}
