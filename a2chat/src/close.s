;
; cc65 close(), with the file buffer released even when MLI CLOSE fails.
; Stdio marks the FILE closed before calling here. If we skip iobuf_free,
; the one FILEIO buffer stays in use and every later fopen returns ENOMEM.
;
        .export         _close
        .import         closedirect, freebuffer, getfd, fdtab
        .import         __mappederrno, __directerrno
        .include        "errno.inc"

        .code

_close:
        jsr     getfd
        bcs     errno
        sty     slot
        bmi     device
        jsr     closedirect
        sta     mlierr
        ldy     slot
        jmp     release
device: lda     #$00
        sta     mlierr
release:
        lda     #$00
        sta     fdtab,y
        jsr     freebuffer
        lda     mlierr
        jmp     __mappederrno

errno:  jmp     __directerrno

        .bss
slot:   .res    1
mlierr: .res    1
