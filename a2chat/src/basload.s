;
; Load an overlay into the idle RX buffer and call it.
; eth_inp is free until the next TCP recv. NBUFS=1, so the
; detokenizer is closed before it opens the BAS file.
;
; A2CHAT.TOK is larger than the buffer. The first $03C0 bytes
; are loaded here; that code reads the rest and closes the file
; before it opens the BAS output.
;
        .export         _bas_ovl
        .export         _bas_tok
        .export         _tok_src
        .export         _tok_end
        .export         _img_src
        .export         _img_end
        .export         fslot
        .export         wbmode
        .import         eth_inp
        .import         _fopen
        .import         _fclose
        .import         _fread
        .import         pushax
        .import         _aux_read
        .import         _aux_write
        .import         _aux_mainbank
        .export         _bas_romsave

; Line image built the way Applesoft SAVE does, and the way appleside
; writes it: next pointer, line number, tokens, one zero. The next
; pointer is one 16-bit add of (body + 4), so a line that starts in
; the last bytes of a page does not come out 257 bytes short.
CLR_RAMRD   = $C002
CLR_RAMWRT  = $C004
SET_RAMWRT  = $C005
RD      = $0380
REND    = $0382
IMG     = $0384
IMS     = $0386
LNK     = $0388
LNM     = $038A
NLIN    = $038C
BIDX    = $038D
TLEN    = $038E
HDR     = $0340

; Tokenizer publishes the aux image range here before it returns.
_img_src        = $02F0
_img_end        = $02F2

        .code

_bas_ovl:
        sta     psave
        stx     psave+1
        lda     #<dtk
        ldx     #>dtk
        ldy     #0
        jmp     openov

_bas_tok:
        sta     psave
        stx     psave+1
        lda     #<tnm
        ldx     #>tnm
        ldy     #1

openov: sty     $02EF
        jsr     pushax
        lda     #<mrb
        ldx     #>mrb
        jsr     _fopen
        sta     fslot
        stx     fslot+1
        ora     fslot+1
        bne     @op
        lda     #0
        tax
        rts
@op:    lda     #<eth_inp
        ldx     #>eth_inp
        jsr     pushax
        lda     #1
        ldx     #0
        jsr     pushax
        lda     #<$03C0
        ldx     #>$03C0
        jsr     pushax
        lda     fslot
        ldx     fslot+1
        jsr     _fread
        cpx     #0
        beq     @bad
        lda     $02EF
        bne     @tok
        lda     fslot
        ldx     fslot+1
        jsr     _fclose
        lda     psave
        ldx     psave+1
        jmp     eth_inp
@tok:   lda     psave
        ldx     psave+1
        jmp     eth_inp
@bad:   lda     fslot
        ldx     fslot+1
        jsr     _fclose
        lda     #0
        tax
        rts

; Tokenise aux[tok_src, tok_end) with the IIc ROM and publish the
; image at $02F0. Returns 1 when at least one numbered line was written.
_bas_romsave:
        lda     _tok_src
        sta     RD
        lda     _tok_src+1
        sta     RD+1
        lda     _tok_end
        sta     REND
        sta     IMS
        sta     IMG
        lda     _tok_end+1
        sta     REND+1
        sta     IMS+1
        sta     IMG+1
        lda     #$01
        sta     LNK
        lda     #$08
        sta     LNK+1
        lda     #0
        sta     NLIN
@ln:    lda     RD+1
        cmp     REND+1
        bcc     @more
        bne     @fin
        lda     RD
        cmp     REND
        bcs     @fin
@more:  jsr     readline
        bcs     @bad
        jsr     linedig
        bcc     @tok
        jmp     @ln
@tok:   jsr     romline
        bcs     @bad
        lda     TLEN
        cmp     #2
        bcc     @ln
        jsr     emitln
        bcs     @bad
        inc     NLIN
        jmp     @ln
@fin:   lda     NLIN
        beq     @bad
        lda     #0
        sta     HDR
        sta     HDR+1
        lda     #<HDR
        sta     $0348
        lda     #>HDR
        sta     $0349
        lda     #2
        jsr     putimg
        bcs     @bad
        lda     IMS
        sta     _img_src
        lda     IMS+1
        sta     _img_src+1
        lda     IMG
        sta     _img_end
        lda     IMG+1
        sta     _img_end+1
        lda     #1
        rts
@bad:   lda     #0
        rts

; One source line into $0200, NUL ended. Carry set if it is too long.
readline:
        lda     REND
        sec
        sbc     RD
        sta     HDR
        lda     REND+1
        sbc     RD+1
        bne     @cap
        lda     HDR
        cmp     #239
        bcc     @n
@cap:   lda     #239
@n:     sta     HDR
        lda     RD
        ldx     RD+1
        jsr     pushax
        lda     #<$0200
        ldx     #>$0200
        jsr     pushax
        lda     HDR
        ldx     #0
        jsr     _aux_read
        jsr     _aux_mainbank
        ldy     #0
@sc:    cpy     HDR
        bcs     @eof
        lda     $0200,y
        cmp     #$0A
        beq     @br
        cmp     #$0D
        beq     @br
        iny
        bne     @sc
@eof:   lda     HDR
        cmp     #239
        bcs     @long
        ldy     HDR
        lda     #0
        sta     $0200,y
        jmp     @adv
@br:    cmp     #$0D
        php
        lda     #0
        sta     $0200,y
        iny
        plp
        bne     @adv
        cpy     HDR
        bcs     @adv
        lda     $0200,y
        cmp     #$0A
        bne     @adv
        iny
@adv:   tya
        clc
        adc     RD
        sta     RD
        bcc     @ok
        inc     RD+1
@ok:    clc
        rts
@long:  sec
        rts

; Carry set if this line is blank or has no number. BIDX is the
; first statement byte. LNM is the line number.
linedig:
        ldy     #0
@sp:    lda     $0200,y
        beq     @skip
        cmp     #' '
        bne     @d
        iny
        bne     @sp
@d:     cmp     #'0'
        bcc     @skip
        cmp     #'9'+1
        bcs     @skip
        lda     #0
        sta     LNM
        sta     LNM+1
@num:   lda     $0200,y
        cmp     #'0'
        bcc     @set
        cmp     #'9'+1
        bcs     @set
        sec
        sbc     #'0'
        sta     HDR+4
        jsr     mul10
        bcs     @skip
        iny
        bne     @num
@set:   sty     BIDX
        clc
        rts
@skip:  sec
        rts

; LNM = LNM*10 + HDR+4. Carry set on overflow.
mul10:  lda     LNM
        sta     HDR+5
        lda     LNM+1
        sta     HDR+6
        asl     LNM
        rol     LNM+1
        bcs     @ov
        lda     LNM
        sta     HDR+7
        lda     LNM+1
        sta     HDR+8
        asl     LNM
        rol     LNM+1
        bcs     @ov
        asl     LNM
        rol     LNM+1
        bcs     @ov
        clc
        lda     LNM
        adc     HDR+7
        sta     LNM
        lda     LNM+1
        adc     HDR+8
        sta     LNM+1
        bcs     @ov
        clc
        lda     LNM
        adc     HDR+4
        sta     LNM
        bcc     @ok
        inc     LNM+1
@ok:    clc
        rts
@ov:    sec
        rts

; In-place ROM parse at $D559. Tokens replace the line at $0200.
romline:
        ldx     #0
@c:     lda     zrst,x
        sta     $0120,x
        inx
        cpx     #zrst_len
        bne     @c
        php
        sei
        sta     CLR_RAMRD
        sta     SET_RAMWRT
        ldx     #0
@s:     lda     $00,x
        sta     $3000,x
        inx
        bne     @s
        sta     CLR_RAMWRT
        lda     #0
        sta     $D6
        lda     BIDX
        sta     $B8
        lda     #$02
        sta     $B9
        bit     $C082
        jsr     $D559
        bit     $C08B
        bit     $C08B
        cld
        ldx     #0
@t:     lda     $0200,x
        beq     @tz
        inx
        bne     @t
        jsr     $0120
        plp
        sec
        rts
@tz:    inx
        stx     TLEN
        jsr     $0120
        plp
        clc
        rts

; Header, then tokens (the terminating 0 included). Carry set if full.
emitln: lda     LNK
        clc
        adc     TLEN
        sta     HDR
        lda     LNK+1
        adc     #0
        sta     HDR+1
        lda     HDR
        clc
        adc     #4
        sta     HDR
        lda     HDR+1
        adc     #0
        sta     HDR+1
        lda     LNM
        sta     HDR+2
        lda     LNM+1
        sta     HDR+3
        lda     HDR
        sta     LNK
        lda     HDR+1
        sta     LNK+1
        lda     #<HDR
        sta     $0348
        lda     #>HDR
        sta     $0349
        lda     #4
        jsr     putimg
        bcs     @no
        lda     #<$0200
        sta     $0348
        lda     #>$0200
        sta     $0349
        lda     TLEN
        jmp     putimg
@no:    rts

; Append A bytes from $0348 (default HDR) onto the aux image.
putimg: sta     $034A
        lda     IMG+1
        cmp     #$80
        bcs     @full
        lda     IMG
        ldx     IMG+1
        jsr     pushax
        lda     $0348
        ldx     $0349
        jsr     pushax
        lda     $034A
        ldx     #0
        jsr     _aux_write
        jsr     _aux_mainbank
        lda     IMG
        clc
        adc     $034A
        sta     IMG
        bcc     @ok
        inc     IMG+1
@ok:    clc
        rts
@full:  sec
        rts

; Runs at $0120, under the RAMRD window. Restores zp from aux $3000.
zrst:   sta     $C003
        ldx     #0
@r:     lda     $3000,x
        sta     $00,x
        inx
        bne     @r
        sta     $C002
        rts
zrst_len = * - zrst

        .rodata
dtk:    .byte   "A2CHAT.DTK", 0
tnm:    .byte   "A2CHAT.TOK", 0
mrb:    .byte   "rb", 0
wbmode: .byte   "wb", 0

        .bss
psave:  .res    2
fslot:  .res    2
_tok_src: .res  2
_tok_end: .res  2
