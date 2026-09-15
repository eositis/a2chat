;
; DS1216E in a slot ROM socket ($Cn00). Slot 3 is 80-column firmware — never probe it.
;
        .export         _nsc_select
        .export         _nsc_read
        .import         popax
        .include        "zeropage.inc"

        .rodata
pat:    .byte   $C5, $3A, $A3, $5C, $C5, $3A, $A3, $5C

        .code

acc:    lda     $C200
        rts

; void __fastcall__ nsc_select(unsigned char slot)
_nsc_select:
        clc
        adc     #$C0
        sta     acc+2
        rts

; unsigned char nsc_read(unsigned char *h, unsigned char *m, unsigned char *s)
_nsc_read:
        sta     ptr3
        stx     ptr3+1
        jsr     popax
        sta     ptr2
        stx     ptr2+1
        jsr     popax
        sta     ptr1
        stx     ptr1+1

        ldx     #7
@pb:    lda     pat,x
        ldy     #8
@pi:    asl     a
        pha
        lda     #0
        bcc     @e
        lda     #1
@e:     sta     acc+1
        jsr     acc
        pla
        dey
        bne     @pi
        dex
        bpl     @pb

        lda     #0
        sta     acc+1
        ldx     #0
@db:    lda     #0
        ldy     #8
@di:    asl     a
        pha
        jsr     acc
        lsr     a
        pla
        rol     a
        dey
        bne     @di
        sta     nd,x
        inx
        cpx     #8
        bne     @db

        lda     nd+6
        and     #$0F
        cmp     #10
        bcs     @no
        lda     nd+6
        cmp     #$60
        bcs     @no
        lda     nd+5
        cmp     #$60
        bcs     @no
        lda     nd+4
        cmp     #$24
        bcs     @no
        ldy     #0
        lda     nd+4
        sta     (ptr1),y
        lda     nd+5
        sta     (ptr2),y
        lda     nd+6
        sta     (ptr3),y
        lda     #1
        ldx     #0
        rts
@no:    lda     #0
        tax
        rts

        .bss
nd:     .res    8
