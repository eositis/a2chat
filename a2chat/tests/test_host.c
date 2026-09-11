#include "a2chat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

static char acc[256];
static unsigned accn;

static void collect(char ch, void *user)
{
    (void)user;
    if (accn + 1 < sizeof(acc)) {
        acc[accn++] = ch;
        acc[accn] = 0;
    }
}

static void feed_file(struct jsonscan *j, const char *path)
{
    FILE *f = fopen(path, "rb");
    int c;
    assert(f);
    while ((c = fgetc(f)) != EOF) {
        jsonscan_feed(j, (char)c);
    }
    fclose(f);
}

int main(void)
{
    struct jsonscan j;
    struct a2cfg c;
    FILE *f;

    jsonscan_init(&j);
    j.on_content = collect;
    accn = 0;
    acc[0] = 0;
    feed_file(&j, "fixtures/stream_hello.ndjson");
    if (strcmp(acc, "Hello world") != 0) {
        fprintf(stderr, "content got '%s'\n", acc);
        return 1;
    }
    if (!j.done) {
        fprintf(stderr, "expected done\n");
        return 1;
    }

    jsonscan_init(&j);
    j.on_content = collect;
    accn = 0;
    acc[0] = 0;
    feed_file(&j, "fixtures/tool_list.ndjson");
    if (!j.has_tool || strcmp(j.tool_name, "list_dir") != 0) {
        fprintf(stderr, "tool name '%s'\n", j.tool_name);
        return 1;
    }
    if (strcmp(j.arg_path, "/HDD") != 0) {
        fprintf(stderr, "path '%s'\n", j.arg_path);
        return 1;
    }

    cfg_defaults(&c);
    cfg_parse_line(&c, "HOST=10.0.0.5");
    cfg_parse_line(&c, "SLOT=4");
    cfg_parse_line(&c, "MAXHIST=8192");
    if (strcmp(c.host, "10.0.0.5") || c.slot != 4 || c.maxhist != 8192) {
        fprintf(stderr, "config parse fail\n");
        return 1;
    }
    cfg_parse_line(&c, "host=10.1.2.3");
    cfg_parse_line(&c, "model=phi3");
    if (strcmp(c.host, "10.1.2.3") || strcmp(c.model, "phi3")) {
        fprintf(stderr, "config case fail\n");
        return 1;
    }
    {
        FILE *cf = fopen("fixtures/cr.cfg", "wb");
        assert(cf);
        fputs("HOST=192.168.4.20\rMODEL=qwen2.5\r", cf);
        fclose(cf);
        if (cfg_load(&c, "fixtures/cr.cfg") != 0 ||
            strcmp(c.host, "192.168.4.20") || strcmp(c.model, "qwen2.5")) {
            fprintf(stderr, "CR cfg load fail host='%s' model='%s'\n", c.host, c.model);
            return 1;
        }
    }

    f = fopen("fixtures/body.json", "w");
    assert(f);
    json_write_prelude(f, "llama3.1");
    fputs("{\"role\":\"user\",\"content\":\"", f);
            json_escape_fwrite(f, "say \"hi\"\n", 9);
    fputs("\"}", f);
    json_write_tools(f);
    json_write_epilogue(f);
    fclose(f);
    f = fopen("fixtures/body.json", "r");
    assert(f);
    {
        char *all;
        long sz;
        fseek(f, 0, SEEK_END);
        sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        all = malloc((size_t)sz + 1);
        fread(all, 1, (size_t)sz, f);
        all[sz] = 0;
        fclose(f);
        if (!strstr(all, "\"model\":\"llama3.1\"") || !strstr(all, "say \\\"hi\\\"") ||
            !strstr(all, "],\"tools\"") || !strstr(all, "list_dir") ||
            all[strlen(all) - 1] != '}') {
            fprintf(stderr, "body assembly fail: %s\n", all);
            free(all);
            return 1;
        }
        free(all);
    }
    {
        FILE *pf = fopen("fixtures/prefix.cfg", "wb");
        assert(pf);
        fputs("PREFIX=/a2.desktop/a2chat/\r", pf);
        fclose(pf);
        if (cfg_load(&c, "fixtures/prefix.cfg") != 0 ||
            strcmp(c.prefix, "/a2.desktop/a2chat") != 0) {
            fprintf(stderr, "prefix normalize fail '%s'\n", c.prefix);
            return 1;
        }
    }
    {
        FILE *in = fopen("fixtures/hi.bas.txt", "wb");
        FILE *tok;
        FILE *out;
        char listing[80];

        assert(in);
        fputs("10 PRINT \"HI\"\n20 END\n", in);
        fclose(in);
        in = fopen("fixtures/hi.bas.txt", "rb");
        tok = fopen("fixtures/hi.tok", "wb");
        assert(in && tok);
        if (bas_tokenize(in, tok) != 0) {
            fprintf(stderr, "tokenize fail\n");
            return 1;
        }
        fclose(in);
        fclose(tok);
        tok = fopen("fixtures/hi.tok", "rb");
        out = fopen("fixtures/hi.list", "wb");
        assert(tok && out);
        if (bas_detokenize(tok, out) != 0) {
            fprintf(stderr, "detokenize fail\n");
            return 1;
        }
        fclose(tok);
        fclose(out);
        in = fopen("fixtures/hi.list", "rb");
        assert(in);
        memset(listing, 0, sizeof(listing));
        fread(listing, 1, sizeof(listing) - 1, in);
        fclose(in);
        if (!strstr(listing, "PRINT") || !strstr(listing, "HI") ||
            !strstr(listing, "20 END")) {
            fprintf(stderr, "roundtrip got '%s'\n", listing);
            return 1;
        }
    }

    puts("host tests ok");
    return 0;
}
