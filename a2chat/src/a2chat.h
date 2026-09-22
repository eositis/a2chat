#ifndef A2CHAT_H
#define A2CHAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>

#define A2CHAT_BUILD 29
#define A2CHAT_BUILD_STR "29"
#define A2CHAT_VERSION "1.1"
#define A2CHAT_OLLA_PORT 40114u
#define A2CHAT_PROMPT_MAX 240
#define A2CHAT_PATH_MAX 48
#define A2CHAT_LINE_MAX 120
#define A2CHAT_TOOL_NAME_MAX 16
#define A2CHAT_DIR_MAX 64
#define A2CHAT_MODEL_MAX 32
#define A2CHAT_WIN 16

struct a2cfg {
    char host[32];
    uint16_t port;
    char model[A2CHAT_MODEL_MAX];
    uint8_t slot; /* 0 = auto */
    char ip[16];
    char gateway[16];
    char netmask[16];
    char prefix[A2CHAT_PATH_MAX];
    uint16_t maxhist;
    uint16_t maxread;
    uint16_t maxwrite;
    uint16_t histcap;
};

struct jsonscan {
    char win[A2CHAT_WIN];
    uint8_t winlen;
    uint8_t winpos;
    uint8_t mode;
    uint8_t escape;
    uint8_t unicode_n;
    uint16_t unicode;
    uint8_t seen_arguments;
    uint8_t seen_tool_calls;
    uint8_t done;
    uint8_t has_tool;
    char tool_name[A2CHAT_TOOL_NAME_MAX];
    uint8_t tool_nlen;
    char arg_path[A2CHAT_PATH_MAX];
    uint8_t path_nlen;
    char arg_type[8];
    uint8_t type_nlen;
    char numbuf[8];
    uint8_t numlen;
    unsigned arg_offset;
    unsigned arg_length;
    unsigned arg_auxtype;
    FILE *pay;
    uint16_t pay_bytes;
    void (*on_content)(char ch, void *user);
    void (*on_span)(const char *p, unsigned n, void *user);
    void *user;
    uint16_t eval_count;
    uint16_t prompt_eval_count;
};

enum {
    JS_SEEK = 0,
    JS_MSG_CONTENT,
    JS_TOOL_NAME,
    JS_ARG_PATH,
    JS_ARG_TYPE,
    JS_ARG_CONTENT,
    JS_ARG_HEX,
    JS_NUM_OFFSET,
    JS_NUM_LENGTH,
    JS_NUM_AUX,
    JS_NUM_EVAL,
    JS_NUM_PROMPT_EVAL
};

extern struct a2cfg g_cfg;
extern uint8_t g_slot;
extern char g_status[81];
extern char g_io80[80];
extern uint8_t g_net_ok;
extern char g_http_err[32];
#ifndef A2CHAT_HOST
extern char g_cfg_loaded[A2CHAT_PATH_MAX];
#endif

/* config.c */
void cfg_defaults(struct a2cfg *c);
int cfg_load(struct a2cfg *c, const char *path);
#ifndef A2CHAT_HOST
int cfg_load_first(struct a2cfg *c);
#endif
int cfg_save(const struct a2cfg *c, const char *path);
void cfg_parse_line(struct a2cfg *c, const char *line);
int cfg_is_olla(const struct a2cfg *c);
const char *cfg_api_path(const struct a2cfg *c, const char *suffix);

/* jsonscan.c */
void jsonscan_init(struct jsonscan *j);
void jsonscan_feed(struct jsonscan *j, char ch);
void jsonscan_feed_buf(struct jsonscan *j, const char *p, unsigned n);
void jsonscan_on_byte(char ch, void *user);
void jsonscan_on_bytes(const char *p, unsigned n, void *user);
void json_escape_fwrite(FILE *f, const char *s, unsigned n);
uint16_t json_escape_aux(uint16_t off, const char *s, unsigned n);
uint16_t aux_add_str(uint16_t off, const char *s);
unsigned json_escape_len(const char *s, unsigned n);

/* jsonesc helpers used by host tests and ollama */
void json_write_prelude(FILE *f, const char *model);
void json_write_tools(FILE *f);
void json_write_epilogue(FILE *f);

int bas_tokenize(FILE *in, FILE *out);
int bas_detokenize(FILE *in, FILE *out);
int bas_header_is_tokenized(const unsigned char *b, unsigned n);
#ifndef A2CHAT_HOST
int bas_detokenize_aux(FILE *in, unsigned max, unsigned *got);
#endif

#ifndef A2CHAT_HOST

void ui_init(void);
void ui_status(const char *msg);
void ui_print(const char *s);
void ui_print_ch(char ch);
void ui_print_n(const char *s, unsigned n);
void ui_flush(void);
void ui_set_perf(const char *s);
void ui_nl(void);
void ui_prompt(char *buf, uint8_t maxlen);
int ui_confirm(const char *path, const char *kind, unsigned bytes);
char ui_getc(void);
int ui_aborted(void);
void ui_redraw_chrome(void);
void ui_label(const char *s);
void ui_clear_chat(void);
void ui_exit(void);

uint8_t slot_resolve(uint8_t configured);
int net_init(uint8_t slot);
void net_shutdown(void);
void net_diag_ollama(void);

int http_post_aux(uint32_t addr, uint16_t port, const char *path,
                  uint16_t json_len,
                  void (*on_bytes)(const char *p, unsigned n, void *user),
                  void *user);
int http_probe_tags(uint32_t addr, uint16_t port);

/* Aux $4000-$BFFF: 32K POST + answer staging. Copy routines live in LC. */
#define A2CHAT_AUX_POST_MAX 0x8000u
#define A2CHAT_PROMPT_AUX   0x7F00u
unsigned char aux_present(void);
#ifndef A2CHAT_HOST
unsigned char __fastcall__ p8_prefix(char *dst);
unsigned char __fastcall__ p8_set_txt(char *path);
#endif
void __fastcall__ aux_write(unsigned off, const unsigned char *src, unsigned n);
void __fastcall__ aux_read(unsigned off, unsigned char *dst, unsigned n);
void __fastcall__ aux_abs_write(unsigned addr, const unsigned char *src, unsigned n);
void __fastcall__ aux_abs_read(unsigned addr, unsigned char *dst, unsigned n);
void aux_mainbank(void);
void prodos_quit(void);

#define CLOCK_NONE  0
#define CLOCK_JIFFY 1
#define CLOCK_NSC   2
#define CLOCK_MFMS  3
uint8_t clock_kind(void);
void clock_init(void);
void clock_wall(char *dst, unsigned dstsz);
void clock_stamp(char *dst, unsigned dstsz);
void clock_reset_ms(void);
uint32_t clock_elapsed_ms(void);
uint32_t clock_post_ms(void);
const char *clock_kind_name(void);

int hist_append(char type, const char *data, uint16_t len);
int hist_append_aux(char type, uint16_t len);
uint16_t hist_emit_json_aux(uint16_t off);
int hist_write_messages(FILE *body);
void hist_new(void);
long hist_size(void);

int prodos_list(const char *path, char *out, unsigned outsz);
int prodos_write_file(const char *path, const char *src_path, uint8_t ptype,
                      unsigned auxtype, int exempt);
int prodos_write_hex_file(const char *path, const char *hex_path, uint8_t ptype,
                          unsigned auxtype, int exempt);
int prodos_write_bas(const char *path, const char *src_path, int exempt);
int prodos_read_to_aux(const char *path, unsigned offset, unsigned length,
                       unsigned *got, int as_hex);
int path_allowed_write(const char *path);
int path_in_workspace(const char *path);
void path_join_prefix(char *dst, const char *in);
void path_join_open(char *dst, const char *in);
void prodos_leaf_name(char *dst, const char *in);
unsigned est_secs_write(unsigned bytes);

int ollama_send(const char *user_text, const char *attach_path);
int cmd_handle(char *line);

char *self_path(const char *filename);

#endif

#endif
