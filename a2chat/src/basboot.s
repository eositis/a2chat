;
; First $03C0 bytes of A2CHAT.TOK. Saves the main-RAM tail the
; tokenizer will cover, parks a restore stub at $50, then hands
; off to a loader at $0300. That loader reads the rest of the
; image. Rodata is staged at $0200 so closing the file (which
; uses $BA00) does not destroy it. One RAMWRT window saves the
; tail. The matching RAMRD window is the stub at $50.
;
        .include        "ovlsym.inc"

        .export         run
        .import         __RESIDENT_LOAD__
        .import         __RESIDENT_SIZE__

CLR_RAMRD       = $C002
SET_RAMRD       = $C003
CLR_RAMWRT      = $C004
SET_RAMWRT      = $C005

        .code

run:    sta     $02FC
        stx     $02FD
        lda     fslot
        sta     $02F4
        lda     fslot+1
        sta     $02F5
        lda     _tok_src
        sta     $02F0
        lda     _tok_src+1
        sta     $02F1
        lda     _tok_end
        sta     $02F2
        lda     _tok_end+1
        sta     $02F3
        ldy     #rs_len-1
@rs:    lda     restore,y
        sta     $50,y
        sta     $0120,y
        dey
        bpl     @rs
        jsr     savetail
        lda     #<__RESIDENT_LOAD__
        sta     ptr1
        lda     #>__RESIDENT_LOAD__
        sta     ptr1+1
        ldy     #0
@c:     lda     (ptr1),y
        sta     $0300,y
        iny
        cpy     #<__RESIDENT_SIZE__
        bne     @c
        jmp     $0300

; $B8F0..$B9EB to aux $2E00. This loop sits below $B8F0.
savetail:
        php
        sei
        sta     CLR_RAMRD
        sta     SET_RAMWRT
        ldx     #0
@a:     lda     $B8F0,x
        sta     $2E00,x
        inx
        cpx     #16
        bne     @a
        ldx     #0
@b:     lda     $B900,x
        sta     $2E10,x
        inx
        cpx     #$EC
        bne     @b
        sta     CLR_RAMWRT
        plp
        rts

; Aux $2E00 back onto $B8F0. Copied to $50 before the tail is covered.
restore:
        php
        sei
        sta     SET_RAMRD
        ldx     #0
@a:     lda     $2E00,x
        sta     $B8F0,x
        inx
        cpx     #16
        bne     @a
        ldx     #0
@b:     lda     $2E10,x
        sta     $B900,x
        inx
        cpx     #$EC
        bne     @b
        sta     CLR_RAMRD
        plp
        lda     $02FA
        ldx     $02FB
        rts
rs_len  = * - restore
.assert rs_len = 42, error, "tokenizer parks this many bytes at $0120"

        .segment        "RESIDENT"

; File position is just after the first $03C0 bytes.
load:   jsr     getc
        sta     $02F6
        jsr     getc
        sta     $02F7
        jsr     getc
        sta     $02F8
        jsr     getc
        sta     $02F9
        lda     #<eth_inp
        ldx     #>eth_inp
        jsr     rdf
        lda     #<$0200
        ldx     #>$0200
        jsr     rdro
        lda     $02F4
        ldx     $02F5
        jsr     _fclose
        ldy     #0
@mv:    cpy     $02F8
        beq     @go
        lda     $0200,y
        sta     $BA00,y
        iny
        bne     @mv
@go:    lda     $02FC
        ldx     $02FD
        jmp     eth_inp

getc:   lda     $02F4
        ldx     $02F5
        jmp     _fgetc

; fread to AX. Count is the code length at $02F6.
rdf:    jsr     pushax
        lda     #1
        ldx     #0
        jsr     pushax
        lda     $02F6
        ldx     $02F7
        jsr     pushax
        lda     $02F4
        ldx     $02F5
        jmp     _fread

; fread to AX. Count is the rodata length at $02F8.
rdro:   jsr     pushax
        lda     #1
        ldx     #0
        jsr     pushax
        lda     $02F8
        ldx     $02F9
        jsr     pushax
        lda     $02F4
        ldx     $02F5
        jmp     _fread
