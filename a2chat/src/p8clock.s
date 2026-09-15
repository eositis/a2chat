;
; Read ProDOS system time at $BF92/$BF93. Do not call GET_TIME and
; do not hook $03FE: clock-card IRQs SED, smash zero page, and run
; from language-card bank 1. Our LC image lives at the same $D400
; range (hist), so a bank switch in IRQ looks like a crash at $DCxx.
;
        .export         _p8_time
        .export         _p8_prefix
        .export         _p8_set_txt
        .import         popax
        .import         _g_io80
        .import         eth_outp
        .include        "zeropage.inc"

; Scratch in g_io80 (idle after fclose): P8 name, GET_FILE_INFO, SET_FILE_INFO.
nmbuf   = _g_io80
giparm  = _g_io80 + 16
siparm  = _g_io80 + 40
; FILEIO $BA00 is free when no file is open (NBUFS=1, closed before TCP).
pfxzp   = eth_outp

MACHID          = $BF98
TIME_MIN        = $BF92
TIME_HOUR       = $BF93

        .code

; unsigned char p8_time(unsigned char *hour, unsigned char *min)
_p8_time:
        sta     ptr2
        stx     ptr2+1
        jsr     popax
        sta     ptr1
        stx     ptr1+1
        ldy     #0
        lda     TIME_HOUR
        and     #$1F
        cmp     #24
        bcs     @no
        sta     (ptr1),y
        lda     TIME_MIN
        and     #$3F
        cmp     #60
        bcs     @no
        sta     (ptr2),y
        ora     TIME_HOUR
        bne     @ok
        lda     MACHID
        lsr     a
        bcc     @no
@ok:    lda     #1
        ldx     #0
        rts
@no:    lda     #0
        tax
        rts

; unsigned char __fastcall__ p8_prefix(char *dst)
; GET_PREFIX ($C7) only. Save cc65 zp around MLI. Do not GET_TIME.
_p8_prefix:
        sta     pfxdst
        stx     pfxdst+1
        sta     pfxparm+1
        stx     pfxparm+2
        php
        sei
        cld
        ldx     #0
@sv:    lda     $80,x
        sta     pfxzp,x
        inx
        cpx     #$20
        bne     @sv
        jsr     $BF00
        .byte   $C7
        .word   pfxparm
        sta     pfxerr
        ldx     #0
@rs:    lda     pfxzp,x
        sta     $80,x
        inx
        cpx     #$20
        bne     @rs
        plp
        lda     pfxdst
        sta     ptr1
        lda     pfxdst+1
        sta     ptr1+1
        lda     pfxerr
        bne     @pfail
        ldy     #0
        lda     (ptr1),y
        beq     @pfail
        sta     pfxn
        cmp     #63
        bcc     @sl0
        lda     #63
        sta     pfxn
@sl0:   ldy     #0
@sl:    iny
        lda     (ptr1),y
        and     #$7F
        dey
        sta     (ptr1),y
        iny
        cpy     pfxn
        bcc     @sl
        beq     @sl
        lda     #0
        ldy     pfxn
        sta     (ptr1),y
        lda     #1
        ldx     #0
        rts
@pfail: lda     #0
        tay
        sta     (ptr1),y
        tax
        rts

; unsigned char __fastcall__ p8_set_txt(char *path)
; GET_FILE_INFO then SET_FILE_INFO file type $04 (TXT). File must be closed.
_p8_set_txt:
        sta     ptr1
        stx     ptr1+1
        ldy     #0
@clen:  lda     (ptr1),y
        beq     @gotn
        iny
        cpy     #15
        bcc     @clen
@gotn:  tya
        bne     @havn
        jmp     @fail
@havn:  sta     nmbuf
        ldy     #0
@cpy:  cpy     nmbuf
        beq     @named
        lda     (ptr1),y
        iny
        sta     nmbuf,y
        clc
        bcc     @cpy
@named: lda     #10
        sta     giparm
        lda     #7
        sta     siparm
        lda     #<nmbuf
        sta     giparm+1
        sta     siparm+1
        lda     #>nmbuf
        sta     giparm+2
        sta     siparm+2
        php
        sei
        cld
        ldx     #0
@sv:    lda     $80,x
        sta     pfxzp,x
        inx
        cpx     #$20
        bne     @sv
        jsr     $BF00
        .byte   $C4
        .word   giparm
        bcs     @mlerr
        lda     giparm+4
        cmp     #$04
        beq     @ok
        lda     giparm+3
        sta     siparm+3
        lda     #$04
        sta     siparm+4
        lda     giparm+5
        sta     siparm+5
        lda     giparm+6
        sta     siparm+6
        lda     #0
        sta     siparm+7
        sta     siparm+8
        sta     siparm+9
        lda     giparm+10
        sta     siparm+10
        lda     giparm+11
        sta     siparm+11
        lda     giparm+12
        sta     siparm+12
        lda     giparm+13
        sta     siparm+13
        jsr     $BF00
        .byte   $C3
        .word   siparm
        bcs     @mlerr
@ok:    lda     #0
@rest:  sta     pfxerr
        ldx     #0
@rs2:   lda     pfxzp,x
        sta     $80,x
        inx
        cpx     #$20
        bne     @rs2
        plp
        lda     pfxerr
        bne     @fail
        lda     #1
        ldx     #0
        rts
@mlerr: lda     #1
        bne     @rest
@fail:  lda     #0
        tax
        rts

        .data
pfxparm:
        .byte   1
        .word   $0000

        .bss
pfxdst: .res    2
pfxerr: .res    1
pfxn:   .res    1
