#ifndef A2CHAT_H
#define A2CHAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>

#define A2CHAT_PATH_MAX 64
#define A2CHAT_LINE_MAX 159
#define A2CHAT_TOOL_NAME_MAX 16
#define A2CHAT_DIR_MAX 64
#define A2CHAT_MODEL_MAX 32
#define A2CHAT_WIN 40

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
    FILE *pay; /* arguments.content / hex streamed here */
    uint16_t pay_bytes;
    void (*on_content)(char ch, void *user);
    void *user;
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
    JS_NUM_AUX
};

extern struct a2cfg g_cfg;
extern uint8_t g_slot;
extern char g_status[81];
extern uint8_t g_net_ok;
extern char g_http_err[48];
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

/* jsonscan.c */
void jsonscan_init(struct jsonscan *j);
void jsonscan_feed(struct jsonscan *j, char ch);
void jsonscan_on_byte(char ch, void *user);
void json_escape_fwrite(FILE *f, const char *s, unsigned n);
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
void ui_flush(void);
void ui_nl(void);
void ui_prompt(char *buf, uint8_t maxlen);
int ui_confirm(const char *path, const char *kind, unsigned bytes);
char ui_getc(void);
int ui_aborted(void);
void ui_redraw_chrome(void);
void ui_label(const char *s);
void ui_clear_chat(void);

uint8_t slot_resolve(uint8_t configured);
int net_init(uint8_t slot);
void net_shutdown(void);
void net_diag_ollama(void);

int http_post_file(uint32_t addr, uint16_t port, const char *path,
                   const char *body_path,
                   void (*on_byte)(char ch, void *user), void *user);
int http_probe_tags(uint32_t addr, uint16_t port);

/* Aux $4000-$7FFF: 16K POST staging. Routines live in LC. */
#define A2CHAT_AUX_POST_MAX 0x4000u
unsigned char aux_present(void);
void __fastcall__ aux_write(unsigned off, const unsigned char *src, unsigned n);
void __fastcall__ aux_read(unsigned off, unsigned char *dst, unsigned n);

int hist_append(char type, const char *data, uint16_t len);
int hist_append_aux(char type, uint16_t len);
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
unsigned est_secs_write(unsigned bytes);

int ollama_send(const char *user_text, const char *attach_path);
int cmd_handle(char *line);

char *self_path(const char *filename);

#endif

#endif
