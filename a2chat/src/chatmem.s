;
; 80-col text-page row copy / pack (STORE80 + PAGE2).
;
        .export         _chat_copy_row
        .export         _chat_pack_row
        .export         _chat_unpack_row
        .import         popa
        .importzp       ptr1, ptr2, tmp1, tmp2, tmp4

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
