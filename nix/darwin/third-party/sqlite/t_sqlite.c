/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "check.h"

typedef struct sqlite3_session sqlite3_session;
typedef struct sqlite3_changeset_iter sqlite3_changeset_iter;
int sqlite3session_create(sqlite3 *, const char *, sqlite3_session **);
int sqlite3session_attach(sqlite3_session *, const char *);
int sqlite3session_changeset(sqlite3_session *, int *, void **);
void sqlite3session_delete(sqlite3_session *);
int sqlite3changeset_start(sqlite3_changeset_iter **, int, void *);
int sqlite3changeset_next(sqlite3_changeset_iter *);
int sqlite3changeset_op(sqlite3_changeset_iter *, const char **, int *, int *, int *);
int sqlite3changeset_finalize(sqlite3_changeset_iter *);
int sqlite3changeset_apply(sqlite3 *, int, void *, int (*)(void *, const char *),
                           int (*)(void *, int, sqlite3_changeset_iter *), void *);
void *sqlite3_preupdate_hook(sqlite3 *, void (*)(void *, sqlite3 *, int, const char *, const char *,
                                                  sqlite3_int64, sqlite3_int64), void *);
int sqlite3_preupdate_count(sqlite3 *);
int sqlite3_preupdate_old(sqlite3 *, int, sqlite3_value **);
int sqlite3_preupdate_new(sqlite3 *, int, sqlite3_value **);

static sqlite3 *db;

static int
row(void *tag, int n, char **v, char **c)
{
    printf("%s:", (const char *)tag);
    for (int i = 0; i < n; i++) {
        printf(" %s=%s", c[i], v[i] ? v[i] : "NULL");
    }
    printf("\n");
    return 0;
}

static int
run(const char *tag, const char *sql)
{
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, row, (void *)tag, &err);
    if (rc != SQLITE_OK) {
        printf("%s: error %d/%d %s\n", tag, rc, sqlite3_extended_errcode(db), err ? err : "");
        sqlite3_free(err);
    }
    return rc;
}

static void
pragmas(void)
{
    static const char *p[] = {
        "auto_vacuum", "automatic_index", "busy_timeout", "cache_size", "cache_spill", "cell_size_check",
        "checkpoint_fullfsync", "defer_foreign_keys", "encoding", "foreign_keys", "fullfsync",
        "hard_heap_limit", "journal_mode", "journal_size_limit", "legacy_alter_table", "locking_mode",
        "max_page_count", "mmap_size", "page_size", "query_only", "read_uncommitted", "recursive_triggers",
        "reverse_unordered_selects", "secure_delete", "soft_heap_limit", "synchronous", "temp_store",
        "threads", "trusted_schema", "wal_autocheckpoint", NULL
    };
    for (int i = 0; p[i] != NULL; i++) {
        char sql[64];
        snprintf(sql, sizeof(sql), "PRAGMA %s", p[i]);
        run("pragma", sql);
    }
}

static void
features(void)
{
    run("types", "SELECT typeof(1), typeof(1.5), typeof('x'), typeof(x'00'), typeof(NULL), 1/2, 1.0/2, 7 % 3, "
        "'10' + 5, CAST('12abc' AS INTEGER), 9223372036854775807 + 0, round(2.5), round(-2.5)");
    run("dqs", "SELECT \"not a column\"");
    run("printf", "SELECT printf('%5.2f|%-4d|%x|%q|%Q|%w', 3.14159, 42, 255, 'it''s', NULL, 'a\"b'), "
        "format('%,d', 1234567), quote(x'cafe'), hex('kido'), unicode('é'), char(0x263a)");
    run("strings", "SELECT upper('aé'), lower('AÉ'), length('aé'), octet_length('aé'), instr('hello','l'), "
        "substr('hello', -3, 2), replace('aaa','a','bb'), trim('  x  '), 'abc' LIKE 'A%', 'abc' GLOB 'A*', "
        "concat_ws('-', 'a', NULL, 'b'), unhex('4b49444f')");
    run("dates", "SELECT date('2026-11-24', '+1 month', 'start of month'), datetime(0, 'unixepoch'), "
        "strftime('%j %W %w %s', '2026-02-28 13:14:15'), julianday('2000-01-01'), "
        "unixepoch('2026-11-24 00:00:00'), timediff('2026-11-24', '2025-01-01')");
    run("json", "SELECT json_extract('{\"a\":[1,2,{\"b\":null}]}', '$.a[2].b'), json_object('k', 1, 'l', json_array(1, 'x')), "
        "'{\"x\":5}' ->> '$.x', '{\"x\":5}' -> 'x', json_valid('{'), json_type('[1]'), "
        "json_patch('{\"a\":1}', '{\"a\":null,\"b\":2}'), json(jsonb('[1,2]'))");
    run("math", "SELECT printf('%.9f %.9f %.9f %.9f', sqrt(2), pi(), ln(10), sin(1)), pow(2, 10), "
        "ceil(-1.5), floor(-1.5), mod(7, 3), log2(1024), log(100)");
    run("ddl", "CREATE TABLE t(id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, score REAL CHECK (score >= 0), "
        "grp TEXT, twice REAL GENERATED ALWAYS AS (score * 2) STORED);"
        "CREATE TABLE s(k TEXT PRIMARY KEY, v INTEGER) STRICT, WITHOUT ROWID;"
        "CREATE INDEX t_grp ON t(grp);"
        "INSERT INTO t(name, score, grp) VALUES ('ann', 3.5, 'a'), ('bob', 1, 'b'), ('cy', 4, 'a'), "
        "('dee', 2.25, 'b'), ('eve', 0, NULL);");
    run("select", "SELECT grp, count(*), sum(score), avg(score), group_concat(name, '+') FROM t GROUP BY grp "
        "ORDER BY grp NULLS LAST");
    run("window", "SELECT name, row_number() OVER w, rank() OVER (ORDER BY score DESC), "
        "sum(score) OVER (PARTITION BY grp ORDER BY id), lag(name) OVER w FROM t WINDOW w AS (ORDER BY id)");
    run("cte", "WITH RECURSIVE f(n, a, b) AS (SELECT 1, 0, 1 UNION ALL SELECT n + 1, b, a + b FROM f WHERE n < 30) "
        "SELECT group_concat(a, ',') FROM f");
    run("upsert", "INSERT INTO t(name, score) VALUES ('bob', 9) ON CONFLICT(name) DO UPDATE SET score = excluded.score "
        "RETURNING id, name, score, twice");
    run("limit", "UPDATE t SET grp = 'z' WHERE grp IS NOT NULL ORDER BY score DESC LIMIT 2; "
        "DELETE FROM t WHERE grp = 'z' ORDER BY id LIMIT 1; SELECT name, grp FROM t ORDER BY id");
    run("strict", "INSERT INTO s VALUES ('x', 'not a number')");
    run("unique", "INSERT INTO t(name, score) VALUES ('ann', 1)");
    run("notnull", "INSERT INTO t(name, score) VALUES (NULL, 1)");
    run("check", "INSERT INTO t(name, score) VALUES ('neg', -1)");
    run("fk", "PRAGMA foreign_keys = ON; CREATE TABLE p(id INTEGER PRIMARY KEY); "
        "CREATE TABLE c(pid INTEGER REFERENCES p(id)); INSERT INTO c VALUES (7)");
    run("unknown", "EXPLAIN SELECT no_such_function(1)");
    run("syntax", "SELEC 1");
    run("alter", "CREATE TABLE r(a); CREATE VIEW rv AS SELECT a FROM r; ALTER TABLE r RENAME TO r2; "
        "SELECT name, sql FROM sqlite_schema WHERE name IN ('r2', 'rv') ORDER BY name");
    run("percentile", "SELECT median(score), percentile(score, 25), percentile_cont(score, 0.5), "
        "percentile_disc(score, 0.5) FROM t");
    run("fts5", "CREATE VIRTUAL TABLE docs USING fts5(body); INSERT INTO docs VALUES ('the quick brown fox'), "
        "('jumps over the lazy dog'), ('quick quick slow'); "
        "SELECT rowid, highlight(docs, 0, '[', ']') FROM docs WHERE docs MATCH 'quick' ORDER BY rowid");
    run("fts4", "CREATE VIRTUAL TABLE d4 USING fts4(body); INSERT INTO d4 VALUES ('alpha beta'), ('beta gamma'); "
        "SELECT docid, snippet(d4) FROM d4 WHERE d4 MATCH 'beta' ORDER BY docid");
    run("rtree", "CREATE VIRTUAL TABLE box USING rtree(id, x0, x1, y0, y1); "
        "INSERT INTO box VALUES (1, 0, 10, 0, 10), (2, 5, 15, 5, 15), (3, 20, 30, 20, 30); "
        "SELECT id FROM box WHERE x1 >= 8 AND x0 <= 12 ORDER BY id");
    run("dbstat", "SELECT name, count(*) FROM dbstat WHERE name IN ('t', 's') GROUP BY name ORDER BY name");
    run("keywords", "SELECT sqlite_compileoption_used('THREADSAFE=2'), sqlite_compileoption_used('ENABLE_FTS5')");
}

static void
limits(void)
{
    static const char *names[] = { "length", "sql_length", "column", "expr_depth", "compound_select", "vdbe_op",
                                   "function_arg", "attached", "like_pattern_length", "variable_number",
                                   "trigger_depth", "worker_threads" };
    for (int i = 0; i < (int)(sizeof(names) / sizeof(names[0])); i++) {
        printf("limit %s %d\n", names[i], sqlite3_limit(db, i, -1));
    }
}

static void
apis(void)
{
    sqlite3_stmt *st = NULL;
    CHECK(sqlite3_prepare_v2(db, "SELECT  a, b FROM  t WHERE id = 5 AND name='x'", -1, &st, NULL) == SQLITE_OK ||
          st == NULL);
    if (st == NULL) {
        sqlite3_prepare_v2(db, "SELECT name, score FROM t WHERE id = 5 AND name = 'x'", -1, &st, NULL);
    }
    const char *norm = sqlite3_normalized_sql(st);
    printf("normalized %s\n", norm ? norm : "NULL");
    sqlite3_finalize(st);

    sqlite3_int64 nums[] = { 3, 1, 4, 1, 5 };
    CHECK(sqlite3_prepare_v2(db, "SELECT sum(value), count(*) FROM carray(?1)", -1, &st, NULL) ==
          SQLITE_OK);
    CHECK(sqlite3_carray_bind(st, 1, nums, 5, CARRAY_INT64, SQLITE_STATIC) == SQLITE_OK);
    CHECK(sqlite3_step(st) == SQLITE_ROW && sqlite3_column_int64(st, 0) == 14);
    sqlite3_reset(st);
    if (sqlite3_step(st) == SQLITE_ROW) {
        printf("carray sum %lld count %d\n", sqlite3_column_int64(st, 0), sqlite3_column_int(st, 1));
    }
    sqlite3_finalize(st);

    sqlite3_blob *blob = NULL;
    run("blob", "CREATE TABLE b(data BLOB); INSERT INTO b VALUES (zeroblob(16))");
    CHECK(sqlite3_blob_open(db, "main", "b", "data", 1, 1, &blob) == SQLITE_OK);
    CHECK(sqlite3_blob_write(blob, "kido", 4, 6) == SQLITE_OK);
    CHECK(sqlite3_blob_bytes(blob) == 16);
    sqlite3_blob_close(blob);
    run("blob", "SELECT hex(data) FROM b");

    char *s = sqlite3_mprintf("%q|%Q|%w|%z", "a'b", "c", "d\"e", sqlite3_mprintf("%d", 7));
    printf("mprintf %s\n", s);
    sqlite3_free(s);
    printf("complete %d %d %d\n", sqlite3_complete("SELECT 1;"), sqlite3_complete("SELECT 'x"),
           sqlite3_complete("CREATE TRIGGER x AFTER INSERT ON t BEGIN SELECT 1;"));
    printf("strglob %d %d strlike %d %d\n", sqlite3_strglob("a*c", "abc"), sqlite3_strglob("A*", "abc"),
           sqlite3_strlike("A%", "abc", 0), sqlite3_strlike("a\\%", "a%", '\\'));
    printf("errstr %s | %s | %s\n", sqlite3_errstr(SQLITE_BUSY), sqlite3_errstr(SQLITE_CONSTRAINT),
           sqlite3_errstr(SQLITE_NOTADB));
    printf("keywords %d\n", sqlite3_keyword_count());

    sqlite3 *copy = NULL;
    CHECK(sqlite3_open(":memory:", &copy) == SQLITE_OK);
    sqlite3_backup *bk = sqlite3_backup_init(copy, "main", db, "main");
    CHECK(bk != NULL && sqlite3_backup_step(bk, -1) == SQLITE_DONE && sqlite3_backup_finish(bk) == SQLITE_OK);
    sqlite3 *saved = db;
    db = copy;
    run("backup", "SELECT count(*), max(name) FROM t");
    db = saved;
    sqlite3_close(copy);

    sqlite3_int64 size = 0;
    unsigned char *image = sqlite3_serialize(db, "main", &size, 0);
    printf("serialize %s %d\n", image != NULL ? "ok" : "failed", size > 0 && size % 4096 == 0);
    sqlite3_free(image);
}

static int changes_seen;

static void
on_preupdate(void *u, sqlite3 *h, int op, const char *dbname, const char *table, sqlite3_int64 k1,
             sqlite3_int64 k2)
{
    (void)u;
    (void)dbname;
    sqlite3_value *v = NULL;
    printf("preupdate op %d table %s keys %lld %lld columns %d", op, table, k1, k2, sqlite3_preupdate_count(h));
    if (op != SQLITE_DELETE && sqlite3_preupdate_new(h, 1, &v) == SQLITE_OK) {
        printf(" new %s", (const char *)sqlite3_value_text(v));
    }
    if (op != SQLITE_INSERT && sqlite3_preupdate_old(h, 1, &v) == SQLITE_OK) {
        printf(" old %s", (const char *)sqlite3_value_text(v));
    }
    printf("\n");
    changes_seen++;
}

static void
session(void)
{
    sqlite3 *a = NULL, *b = NULL;
    sqlite3_session *ses = NULL;
    CHECK(sqlite3_open(":memory:", &a) == SQLITE_OK && sqlite3_open(":memory:", &b) == SQLITE_OK);
    const char *schema = "CREATE TABLE kv(k INTEGER PRIMARY KEY, v TEXT)";
    sqlite3_exec(a, schema, NULL, NULL, NULL);
    sqlite3_exec(b, schema, NULL, NULL, NULL);
    sqlite3_exec(a, "INSERT INTO kv VALUES (1, 'one')", NULL, NULL, NULL);
    sqlite3_exec(b, "INSERT INTO kv VALUES (1, 'one')", NULL, NULL, NULL);
    sqlite3_preupdate_hook(b, on_preupdate, NULL);
    sqlite3_exec(b, "INSERT INTO kv VALUES (9, 'nine'); UPDATE kv SET v = 'neun' WHERE k = 9; "
                    "DELETE FROM kv WHERE k = 9", NULL, NULL, NULL);
    sqlite3_preupdate_hook(b, NULL, NULL);
    CHECK(sqlite3session_create(a, "main", &ses) == SQLITE_OK && sqlite3session_attach(ses, NULL) == SQLITE_OK);
    sqlite3_exec(a, "INSERT INTO kv VALUES (2, 'two'); UPDATE kv SET v = 'uno' WHERE k = 1; "
                    "DELETE FROM kv WHERE k = 2; INSERT INTO kv VALUES (3, 'three')", NULL, NULL, NULL);
    int n = 0;
    void *cs = NULL;
    CHECK(sqlite3session_changeset(ses, &n, &cs) == SQLITE_OK && n > 0);
    sqlite3_changeset_iter *it = NULL;
    CHECK(sqlite3changeset_start(&it, n, cs) == SQLITE_OK);
    while (sqlite3changeset_next(it) == SQLITE_ROW) {
        const char *table;
        int cols, op, indirect;
        sqlite3changeset_op(it, &table, &cols, &op, &indirect);
        printf("changeset %s op %d columns %d\n", table, op, cols);
    }
    sqlite3changeset_finalize(it);
    CHECK(sqlite3changeset_apply(b, n, cs, NULL, NULL, NULL) == SQLITE_OK);
    sqlite3 *saved = db;
    db = b;
    run("session", "SELECT k, v FROM kv ORDER BY k");
    db = saved;
    sqlite3_free(cs);
    sqlite3session_delete(ses);
    sqlite3_close(a);
    sqlite3_close(b);
    CHECK(changes_seen == 3);
}

static void
wal(const char *dir)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/t_sqlite-wal.db", dir);
    unlink(path);
    sqlite3 *w = NULL;
    CHECK(sqlite3_open_v2(path, &w, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) == SQLITE_OK);
    sqlite3 *saved = db;
    db = w;
    run("wal", "PRAGMA journal_mode = WAL");
    run("wal", "CREATE TABLE x(a); INSERT INTO x VALUES (1), (2); PRAGMA wal_checkpoint(TRUNCATE)");
    run("wal", "PRAGMA synchronous");
    db = saved;
    sqlite3_close(w);
    char other[1100];
    snprintf(other, sizeof(other), "file:%s?mode=ro", path);
    CHECK(sqlite3_open_v2(other, &w, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, NULL) == SQLITE_OK);
    db = w;
    run("uri", "SELECT count(*) FROM x");
    CHECK(run("uri", "INSERT INTO x VALUES (3)") == SQLITE_READONLY);
    db = saved;
    sqlite3_close(w);
    unlink(path);
    snprintf(other, sizeof(other), "%s-wal", path);
    unlink(other);
    snprintf(other, sizeof(other), "%s-shm", path);
    unlink(other);
}

int
main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "/tmp";
    printf("version %s %d\n", sqlite3_libversion(), sqlite3_libversion_number());
    printf("source %s\n", sqlite3_sourceid());
    CHECK(sqlite3_libversion_number() >= 3054000);
    CHECK(sqlite3_threadsafe() == 2);
    for (int i = 0; sqlite3_compileoption_get(i) != NULL; i++) {
        printf("option %s\n", sqlite3_compileoption_get(i));
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s/t_sqlite.db", dir);
    unlink(path);
    CHECK(sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) == SQLITE_OK);
    pragmas();
    features();
    limits();
    apis();
    session();
    sqlite3_close(db);
    unlink(path);
    wal(dir);
    return checks_done("sqlite");
}
