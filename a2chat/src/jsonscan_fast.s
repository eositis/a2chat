;
; 65C02 inner loop for NDJSON message.content (plain ASCII, no escape).
; Offsets must match struct jsonscan in a2chat.h (A2CHAT_WIN=16).
;
        .export         _jsonscan_content_run
        .import         _jsonscan_emit_ch, popax
        .include        "zeropage.inc"

JS_MODE         = 18
JS_ESCAPE       = 19
JS_UNICN        = 20
JS_MSG_CONTENT  = 1

        .bss
emitc:  .res    1

        .segment        "LC"

; unsigned __fastcall__ jsonscan_content_run(struct jsonscan *j,
;                                            const char *p, unsigned n)
_jsonscan_content_run:
        sta     tmp1
        stx     tmp2
        jsr     popax
        sta     ptr1
        stx     ptr1+1
        jsr     popax
        sta     ptr2
        stx     ptr2+1

        lda     tmp1
        ora     tmp2
        bne     chkmode
        jmp     ret0
chkmode:
        ldy     #JS_MODE
        lda     (ptr2),y
        cmp     #JS_MSG_CONTENT
        beq     :+
        jmp     ret0
:       ldy     #JS_ESCAPE
        lda     (ptr2),y
        beq     :+
        jmp     ret0
:       ldy     #JS_UNICN
        lda     (ptr2),y
        beq     :+
        jmp     ret0
:

        lda     #0
        sta     tmp3
        sta     tmp4

loop:
        lda     tmp1
        ora     tmp2
        beq     done

        ldy     #0
        lda     (ptr1),y
        cmp     #$5C
        beq     done
        cmp     #'"'
        beq     done

        sta     emitc
        lda     ptr1
        pha
        lda     ptr1+1
        pha
        lda     ptr2
        pha
        lda     ptr2+1
        pha
        lda     tmp1
        pha
        lda     tmp2
        pha
        lda     tmp3
        pha
        lda     tmp4
        pha
        lda     emitc
        jsr     _jsonscan_emit_ch
        pla
        sta     tmp4
        pla
        sta     tmp3
        pla
        sta     tmp2
        pla
        sta     tmp1
        pla
        sta     ptr2+1
        pla
        sta     ptr2
        pla
        sta     ptr1+1
        pla
        sta     ptr1

        inc     ptr1
        bne     :+
        inc     ptr1+1
:
        inc     tmp3
        bne     :+
        inc     tmp4
:
        lda     tmp1
        bne     :+
        dec     tmp2
:       dec     tmp1
        jmp     loop

done:
        lda     tmp3
        ldx     tmp4
        rts

ret0:
        lda     #0
        tax
        rts
