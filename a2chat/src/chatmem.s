;
; 80-col text-page row copy / pack (STORE80 + PAGE2).
;
        .export         _chat_copy_row
        .export         _chat_pack_row
        .export         _chat_unpack_row
        .export         _chat_write
        .export         _line_put_front
        .import         popa
        .import         popax
        .importzp       ptr1, ptr2, tmp1, tmp2, tmp3, tmp4

STORE80_ON      = $C001
PAGE2_OFF       = $C054
PAGE2_ON        = $C055

        .code

; A = row. Result ptr1 = $0400 + (row&7)*$80 + (row>>3)*40
text_base:
        sta     tmp4
        and     #7
        sta     ptr1
        lda     #0
        sta     ptr1+1
        ldx     #7
:       asl     ptr1
        rol     ptr1+1
        dex
        bne     :-
        lda     tmp4
        lsr
        lsr
        lsr
        sta     tmp1
        asl
        asl
        asl
        sta     tmp2
        lda     tmp1
        asl
        asl
        asl
        asl
        asl
        clc
        adc     tmp2
        adc     ptr1
        sta     ptr1
        lda     ptr1+1
        adc     #0
        adc     #4
        sta     ptr1+1
        rts

copy40:
        ldy     #39
:       lda     (ptr2),y
        sta     (ptr1),y
        dey
        bpl     :-
        rts

; void __fastcall__ chat_copy_row(uint8_t dst, uint8_t src)
_chat_copy_row:
        jsr     text_base
        lda     ptr1
        sta     ptr2
        lda     ptr1+1
        sta     ptr2+1
        jsr     popa
        jsr     text_base
        sta     STORE80_ON
        sta     PAGE2_ON
        jsr     copy40
        sta     PAGE2_OFF
        jmp     copy40

; void __fastcall__ chat_write(uint8_t col, uint8_t row, const char *s, uint8_t n)
; Normal text only. Even columns, then odd columns.
_chat_write:
        pha
        jsr     popax
        sta     ptr2
        stx     ptr2+1
        jsr     popa
        pha
        jsr     popa
        sta     tmp3
        pla
        jsr     text_base
        pla
        beq     @none
        sta     tmp1
        lda     #0
        sta     tmp4
        sta     STORE80_ON
        sta     PAGE2_ON
        jsr     wrpass
        sta     PAGE2_OFF
        lda     #1
        sta     tmp4
        jsr     wrpass
@none:  rts

; tmp1 = n, tmp3 = col, tmp4 = parity (0 even, 1 odd)
; ptr1 = row base, ptr2 = source. tmp2 holds the column offset.
wrpass:
        ldy     #0
@lp:    cpy     tmp1
        bcs     @out
        tya
        clc
        adc     tmp3
        and     #1
        cmp     tmp4
        bne     @next
        tya
        clc
        adc     tmp3
        lsr     a
        sta     tmp2
        lda     (ptr2),y
        and     #$7f
        cmp     #32
        bcs     @glyph
        lda     #' '
@glyph: ora     #$80
        phy
        ldy     tmp2
        sta     (ptr1),y
        ply
@next:  iny
        jmp     @lp
@out:   rts

scatter:
        ldy     #0
@s1:    lda     (ptr1),y
        sta     (ptr2)
        inc     ptr2
        bne     @s2
        inc     ptr2+1
@s2:    inc     ptr2
        bne     @s3
        inc     ptr2+1
@s3:    iny
        cpy     #40
        bne     @s1
        rts

gather:
        ldy     #0
@g1:    lda     (ptr2)
        sta     (ptr1),y
        inc     ptr2
        bne     @g2
        inc     ptr2+1
@g2:    inc     ptr2
        bne     @g3
        inc     ptr2+1
@g3:    iny
        cpy     #40
        bne     @g1
        rts

; text_base clobbers tmp1/tmp2/tmp4/ptr1. Keep C buffer in ptr2 until
; after that, then snapshot it in tmp1/tmp2 for the odd-column pass.
; void __fastcall__ chat_pack_row(uint8_t row, unsigned char *dst)
_chat_pack_row:
        sta     ptr2
        stx     ptr2+1
        jsr     popa
        jsr     text_base
        lda     ptr2
        sta     tmp1
        lda     ptr2+1
        sta     tmp2
        sta     STORE80_ON
        sta     PAGE2_ON
        jsr     scatter
        lda     tmp1
        clc
        adc     #1
        sta     ptr2
        lda     tmp2
        adc     #0
        sta     ptr2+1
        sta     PAGE2_OFF
        jmp     scatter

; void __fastcall__ chat_unpack_row(uint8_t row, const unsigned char *src)
_chat_unpack_row:
        sta     ptr2
        stx     ptr2+1
        jsr     popa
        jsr     text_base
        lda     ptr2
        sta     tmp1
        lda     ptr2+1
        sta     tmp2
        sta     STORE80_ON
        sta     PAGE2_ON
        jsr     gather
        lda     tmp1
        clc
        adc     #1
        sta     ptr2
        lda     tmp2
        adc     #0
        sta     ptr2+1
        sta     PAGE2_OFF
        jmp     gather

; void __fastcall__ line_put_front(char *s)
; Rotate a "/load" token to the start of the line.
; "query /load NAME" -> "/load NAME query"
_line_put_front:
        sta     ptr1
        stx     ptr1+1
        ldy     #0
@scan:  lda     (ptr1),y
        beq     @out
        cmp     #'/'
        bne     @adv
        cpy     #0
        beq     @edge
        dey
        lda     (ptr1),y
        iny
        cmp     #' '
        bne     @adv
@edge:  sty     tmp1
        iny
        lda     (ptr1),y
        cmp     #'l'
        bne     @back
        iny
        lda     (ptr1),y
        cmp     #'o'
        bne     @back
        iny
        lda     (ptr1),y
        cmp     #'a'
        bne     @back
        iny
        lda     (ptr1),y
        cmp     #'d'
        bne     @back
        iny
        lda     (ptr1),y
        beq     @hit
        cmp     #' '
        beq     @hit
@back:  ldy     tmp1
@adv:   iny
        bne     @scan
@out:   rts
@hit:   ldy     tmp1
        beq     @out
        ldy     #0
@len:   lda     (ptr1),y
        beq     @got
        iny
        bne     @len
@got:   sty     tmp2
        lda     tmp2
        sec
        sbc     tmp1
        sta     tmp4
@rot:   lda     tmp1
        beq     @gap
        ldy     #0
        lda     (ptr1),y
        tax
@sl:    iny
        lda     (ptr1),y
        dey
        sta     (ptr1),y
        iny
        cpy     tmp2
        bne     @sl
        ldy     tmp2
        dey
        txa
        sta     (ptr1),y
        dec     tmp1
        jmp     @rot
@gap:   ldy     tmp2
        dey
        dey
@sh:    cpy     tmp4
        bcc     @sp
        lda     (ptr1),y
        iny
        sta     (ptr1),y
        dey
        dey
        cpy     #$ff
        bne     @sh
@sp:    ldy     tmp4
        lda     #' '
        sta     (ptr1),y
        rts
