; Tokenize an Applesoft listing into a ProDOS BAS image.
; Source text is aux at $4000 + _tok_src, ending at $4000 + _tok_end.
; The token image is written just after that text, then saved.
;
; Language card: one switch to ROM, copy the keyword table, switch back.
; Aux: one RAMRD window per source line, one RAMWRT window per token line.
;
        .include        "ovlsym.inc"

        .export         run

CLR_RAMRD       = $C002
SET_RAMRD       = $C003
CLR_RAMWRT      = $C004
SET_RAMWRT      = $C005
LINE            = $0200
TAB             = $BB00
TBUF            = $BD00

        .code

; Switch to Applesoft ROM, copy the keyword table, switch back to ProDOS.
; This lives below $B8F0. The bytes at $B9B9/$B9BB are printf workspace in
; the main binary; a zero written there turns the following opcode into BRK.
rom_tab:
        php
        sei
        sta     CLR_RAMRD
        sta     CLR_RAMWRT
        bit     $C082
        ldy     #0
@p1:    lda     $D0D0,y
        sta     $BB00,y
        iny
        bne     @p1
        bit     $C082
@p2:    lda     $D1D0,y
        sta     $BC00,y
        iny
        bne     @p2
        bit     $C08B
        bit     $C08B
        plp
        rts

; Path arrives in AX. Offsets were stored at $02F0 by the loader.
; Exit jumps through the restore stub at $50, which puts the main-RAM
; tail back and returns to the C caller.
run:    sta     ppath
        stx     ppath+1
        ldy     #fetch_len-1
@f:     lda     fetch,y
        sta     $50,y
        dey
        bpl     @f
        jsr     rom_tab
        jsr     instubs
        lda     TAB
        cmp     #'E'
        beq     @tab
        jmp     fail
@tab:
        lda     TAB+1
        cmp     #'N'
        bne     fail
        lda     TAB+2
        cmp     #$C4
        bne     fail
        lda     $02F0
        sta     spos
        lda     $02F1
        clc
        adc     #$40
        sta     spos+1
        lda     $02F2
        sta     send
        lda     $02F3
        clc
        adc     #$40
        sta     send+1
        ldx     #1
@di:    lda     send,x
        sta     dpos,x
        sta     img,x
        dex
        bpl     @di
        lda     #$01
        sta     link
        lda     #$08
        sta     link+1
        lda     #0
        sta     nlines

@ln:    jsr     src_done
        bcs     @end
        jsr     readln
        bcs     fail
        jsr     tokline
        cmp     #2
        beq     fail
        cmp     #1
        bne     @ln
        inc     nlines
        bne     @ln
@end:   lda     nlines
        beq     fail
        lda     #0
        jsr     putb
        lda     #0
        jsr     putb
        bcs     fail
        jmp     publish

fail:   lda     #0
        tax
exit:   sta     $02FA
        stx     $02FB
        ldy     #41
@u:     lda     $0120,y
        sta     $50,y
        dey
        bpl     @u
        jmp     $50

; Carry set when spos >= send.
src_done:
        lda     spos+1
        cmp     send+1
        bcc     @no
        bne     @yes
        lda     spos
        cmp     send
        bcs     @yes
@no:    clc
        rts
@yes:   sec
        rts

; One aux read window. Line (no CR) lands at $0200, length in llen.
readln: lda     spos
        sta     ptr1
        lda     spos+1
        sta     ptr1+1
        lda     send
        sta     ptr3
        lda     send+1
        sta     ptr3+1
        jsr     $0300
        php
        lda     ptr1
        sta     spos
        lda     ptr1+1
        sta     spos+1
        plp
        rts

; 0 = skipped, 1 = wrote a line, 2 = error.
tokline:
        lda     #0
        sta     npos
        sta     in_str
        sta     rem
        sta     tn
        jsr     skipsp
        ldy     npos
        cpy     llen
        bcc     @has
        jmp     @skip
@has:   lda     LINE,y
        cmp     #'0'
        bcs     @dig
        jmp     @skip
@dig:   cmp     #'9'+1
        bcc     @num
        jmp     @skip
@num:   jsr     parsenum
        bcc     @okn
        jmp     @bad
@okn:   jsr     skipsp
@st:    lda     npos
        cmp     llen
        bcc     @body
        jmp     @emit
@body:
        lda     rem
        ora     in_str
        bne     @lit
        ldy     npos
        lda     LINE,y
        cmp     #'"'
        beq     @quo
        cmp     #'?'
        beq     @prn
        jsr     match
        lda     bestn
        beq     @raw
        lda     bestt
        ora     #$80
        jsr     puttok
        bcs     @bad
        cmp     #$B2
        beq     @rem
        cmp     #$83
        bne     @add
@rem:   lda     #1
        sta     rem
@add:   lda     npos
        clc
        adc     bestn
        sta     npos
        jmp     @st
@raw:   ldy     npos
        lda     LINE,y
        jsr     puttok
        bcs     @bad
        inc     npos
        jmp     @st
@prn:   lda     #$BA
        jsr     puttok
        bcs     @bad
        inc     npos
        jmp     @st
@quo:   lda     #1
        sta     in_str
        lda     #'"'
        jsr     puttok
        bcs     @bad
        inc     npos
        jmp     @st
@lit:   ldy     npos
        lda     LINE,y
        cmp     #'"'
        bne     @cp
        lda     in_str
        beq     @cp
        lda     rem
        bne     @cp
        lda     #0
        sta     in_str
@cp:    ldy     npos
        lda     LINE,y
        jsr     puttok
        bcs     @bad
        inc     npos
        jmp     @st
@emit:  jsr     wrline
        bcs     @bad
        lda     #1
        rts
@skip:  lda     #0
        rts
@bad:   lda     #2
        rts

skipsp: ldy     npos
        cpy     llen
        bcs     @r
        lda     LINE,y
        cmp     #' '
        beq     @n
        cmp     #$09
        bne     @r
@n:     inc     npos
        jmp     skipsp
@r:     rts

parsenum:
        lda     #0
        sta     lnum
        sta     lnum+1
@d:     ldy     npos
        cpy     llen
        bcs     @ck
        lda     LINE,y
        cmp     #'0'
        bcc     @ck
        cmp     #'9'+1
        bcs     @ck
        sec
        sbc     #'0'
        sta     tmp1
        jsr     mul10
        bcs     @bad
        inc     npos
        jmp     @d
@ck:    lda     lnum+1
        cmp     #$FA
        bcs     @bad
        clc
        rts
@bad:   sec
        rts

; lnum = lnum*10 + tmp1. Carry set on overflow.
mul10:  lda     lnum
        sta     tmp2
        lda     lnum+1
        sta     tmp3
        asl     lnum
        rol     lnum+1
        bcs     @ov
        lda     lnum
        sta     tmp4
        lda     lnum+1
        pha
        asl     lnum
        rol     lnum+1
        bcs     @ovp
        asl     lnum
        rol     lnum+1
        bcs     @ovp
        clc
        lda     lnum
        adc     tmp4
        sta     lnum
        pla
        adc     lnum+1
        sta     lnum+1
        bcs     @ov
        clc
        lda     lnum
        adc     tmp1
        sta     lnum
        lda     lnum+1
        adc     #0
        sta     lnum+1
        rts
@ovp:   pla
@ov:    sec
        rts

puttok: ldx     tn
        cpx     #240
        bcs     @no
        sta     TBUF,x
        inc     tn
        clc
        rts
@no:    sec
        rts

; Longest keyword at LINE+npos. bestn = length, bestt = index ($80 + index).
match:  lda     #0
        sta     bestn
        sta     bestt
        sta     kwi
        lda     #<TAB
        sta     ptr4
        lda     #>TAB
        sta     ptr4+1
@kw:    ldy     #0
        lda     (ptr4),y
        beq     @done
        ldx     #0
        lda     #1
        sta     tmp1
@ch:            lda     (ptr4),y
        pha
        and     #$7F
        sta     tmp2
        jsr     schar
        cmp     tmp2
        beq     @eq
        lda     #0
        sta     tmp1
@eq:    inx
        pla
        bmi     @end
        iny
        bne     @ch
@end:   lda     tmp1
        beq     @adv
        lda     tmp2
        jsr     wordend
        bcc     @take
        jsr     schar
        jsr     ident
        bcs     @adv
@take:  cpx     bestn
        bcc     @adv
        stx     bestn
        lda     kwi
        sta     bestt
@adv:   txa
        clc
        adc     ptr4
        sta     ptr4
        bcc     @nh
        inc     ptr4+1
@nh:            inc     kwi
        jmp     @kw
@done:  rts

; X = offset from npos. A = uppercase char, or 0 past the line. X/Y kept.
schar:  stx     tmp3
        sty     tmp4
        txa
        clc
        adc     npos
        cmp     llen
        bcs     @z
        tay
        lda     LINE,y
        jsr     upch
        jmp     @r
@z:     lda     #0
@r:     ldx     tmp3
        ldy     tmp4
        rts

upch:   cmp     #'a'
        bcc     @k
        cmp     #'z'+1
        bcs     @k
        sec
        sbc     #32
@k:     rts

; Carry set if A is A-Z or '$'.
wordend:
        cmp     #'A'
        bcc     @no
        cmp     #'Z'+1
        bcc     @yes
        cmp     #'$'
        beq     @yes
@no:    clc
        rts
@yes:   sec
        rts

ident:  cmp     #'0'
        bcc     @no
        cmp     #'9'+1
        bcc     @yes
        cmp     #'A'
        bcc     @no
        cmp     #'Z'+1
        bcc     @yes
        cmp     #'a'
        bcc     @no
        cmp     #'z'+1
        bcc     @yes
        cmp     #'$'
        beq     @yes
@no:    clc
        rts
@yes:   sec
        rts

; Header, tokens, and the ending 0. One RAMWRT window. Carry set if no room.
wrline: lda     link
        clc
        adc     tn
        sta     hdr
        lda     link+1
        adc     #0
        sta     hdr+1
        lda     hdr
        clc
        adc     #5
        sta     hdr
        lda     hdr+1
        adc     #0
        sta     hdr+1
        lda     lnum
        sta     hdr+2
        lda     lnum+1
        sta     hdr+3
        lda     dpos
        sta     ptr2
        lda     dpos+1
        sta     ptr2+1
        jsr     $0380
        lda     ptr2
        sta     dpos
        lda     ptr2+1
        sta     dpos+1
        bcs     @no
        lda     hdr
        sta     link
        lda     hdr+1
        sta     link+1
        clc
@no:    rts

; A = byte appended to the image. Carry set if aux is full.
putb:   sta     tmp2
        lda     dpos
        sta     ptr2
        lda     dpos+1
        sta     ptr2+1
        jsr     $03C0
        lda     ptr2
        sta     dpos
        lda     ptr2+1
        sta     dpos+1
        rts

; Aux offsets of the token image, for the C writer after this overlay exits.
publish:
        sta     CLR_RAMRD
        sta     CLR_RAMWRT
        lda     img
        sta     $02F0
        lda     img+1
        sec
        sbc     #$40
        sta     $02F1
        lda     dpos
        sta     $02F2
        lda     dpos+1
        sec
        sbc     #$40
        sta     $02F3
        lda     #1
        ldx     #0
        jmp     exit

; One copy. Line reader at $0300, line writer at $0380, byte writer at
; $03B0, byte reader at $03C0. $03D0 is left for ProDOS.
instubs:
        ldy     #0
@r:     lda     stub_rd,y
        sta     $0300,y
        iny
        cpy     #stub_rd_len
        bne     @r
        ldy     #0
@w:     lda     stub_wr,y
        sta     $0380,y
        iny
        cpy     #stub_wr_len
        bne     @w
        ldy     #0
@b:     lda     stub_pb,y
        sta     $03C0,y
        iny
        cpy     #stub_pb_len
        bne     @b
        rts

        .rodata

; Copied to $0300/$0380/$03C0 before any file is opened. Relative
; branches stay valid. RAMRD/RAMWRT wrap the whole line, then switch back.
stub_rd:
        php
        sei
        ldx     #0
@c:     lda     ptr1+1
        cmp     ptr3+1
        bcc     @rd
        bne     @end
        lda     ptr1
        cmp     ptr3
        bcs     @end
@rd:    jsr     $50
        inc     ptr1
        bne     @ni
        inc     ptr1+1
@ni:    cmp     #$0D
        beq     @nl
        cmp     #$0A
        beq     @nl
        cpx     #239
        bcs     @long
        sta     LINE,x
        inx
        bne     @c
@nl:    sta     tmp1
        lda     ptr1+1
        cmp     ptr3+1
        bcc     @pk
        bne     @end
        lda     ptr1
        cmp     ptr3
        bcs     @end
@pk:    jsr     $50
        cmp     #$0D
        beq     @sw
        cmp     #$0A
        bne     @end
@sw:    cmp     tmp1
        beq     @end
        inc     ptr1
        bne     @end
        inc     ptr1+1
@end:   lda     #0
        sta     LINE,x
        stx     llen
        plp
        clc
        rts
@long:  plp
        sec
        rts
stub_rd_len = * - stub_rd
.assert stub_rd_len <= $80, error, "line reader overlaps $0380"

stub_wr:
        php
        sei
        sta     SET_RAMWRT
        ldy     #0
@h:     lda     hdr,y
        jsr     rawput
        bcs     @full
        iny
        cpy     #4
        bne     @h
        ldy     #0
@t:     cpy     tn
        beq     @z
        lda     TBUF,y
        jsr     rawput
        bcs     @full
        iny
        bne     @t
@z:     lda     #0
        jsr     rawput
        bcs     @full
        sta     CLR_RAMWRT
        plp
        clc
        rts
@full:  sta     CLR_RAMWRT
        plp
        sec
        rts
stub_wr_len = * - stub_wr
.assert stub_wr_len <= $40, error, "line writer overlaps $03C0"

; Called from the write stubs. RAMWRT is already on. Y is kept in tmp1.
rawput: pha
        lda     ptr2+1
        cmp     #$C0
        bcs     @nf
        pla
        sty     tmp1
        ldy     #0
        sta     (ptr2),y
        inc     ptr2
        bne     @ok
        inc     ptr2+1
@ok:    ldy     tmp1
        clc
        rts
@nf:    pla
        sec
        rts

stub_pb:
        php
        sei
        sta     SET_RAMWRT
        lda     tmp2
        jsr     rawput
        sta     CLR_RAMWRT
        plp
        rts
stub_pb_len = * - stub_pb
.assert stub_pb_len <= $10, error, "byte writer hits ProDOS"

; One aux byte at (ptr1) into A. Runs at $50: RAMRD would fetch
; the next opcode from aux if this lived at $0300.
fetch:  sta     SET_RAMRD
        ldy     #0
        lda     (ptr1),y
        sta     CLR_RAMRD
        rts
fetch_len = * - fetch

        .bss
ppath:  .res    2
spos:   .res    2
send:   .res    2
dpos:   .res    2
img:    .res    2
cur:    .res    2
link:   .res    2
lnum:   .res    2
hdr:    .res    4
fhand:  .res    2
llen:   .res    1
npos:   .res    1
tn:     .res    1
nlines: .res    1
in_str: .res    1
rem:    .res    1
bestn:  .res    1
bestt:  .res    1
kwi:    .res    1
