;
; MegaFlash $C0C0 timers and presence (slot 4 I/O, not Uthernet $C0C4).
;
        .export         _mf_present
        .export         _mf_reset_ms
        .export         _mf_get_ms
        .export         _mf_timestr
        .importzp       sreg, ptr1

cmdreg          = $C0C0
paramreg        = $C0C1
idreg           = $C0C3
CMD_GETDEVINFO  = $10
CMD_GETTIMESTR  = $19
CMD_RESETTIMER_MS = $42
CMD_GETTIMER_MS = $43
BUSYFLAG        = $80
ERRORFLAG       = $40

        .code

; Wait while busy. C=1 timeout/error, C=0 ok.
mf_wait:
        ldx     #$00
:       bit     cmdreg
        bpl     @idle
        dex
        bne     :-
        sec
        rts
@idle:  bit     cmdreg
        bvs     @err
        clc
        rts
@err:   sec
        rts

; unsigned char mf_present(void)
; Only the ID toggle. Do not write $C0C0 here: on a slot-4 Uthernet II
; without MegaFlash that location is the W5100 mode register.
_mf_present:
        lda     idreg
        eor     idreg
        inc     a
        bne     @no
        lda     #1
        ldx     #0
        rts
@no:    lda     #0
        tax
        rts

; void mf_reset_ms(void)
_mf_reset_ms:
        lda     #CMD_RESETTIMER_MS
        sta     cmdreg
        jmp     mf_wait

; uint32_t mf_get_ms(void)  AX=low, sreg=high
_mf_get_ms:
        lda     #CMD_GETTIMER_MS
        sta     cmdreg
        jsr     mf_wait
        bcs     @z
        lda     paramreg
        sta     tmp1
        lda     paramreg
        sta     tmp2
        lda     paramreg
        sta     sreg
        lda     paramreg
        sta     sreg+1
        lda     tmp1
        ldx     tmp2
        rts
@z:     lda     #0
        tax
        sta     sreg
        sta     sreg+1
        rts

; unsigned char __fastcall__ mf_timestr(char *dst)
; MegaFlash CMD_GETTIMESTR: 8 inverse-ASCII chars, typically "HH:MM AM"
_mf_timestr:
        sta     ptr1
        stx     ptr1+1
        lda     #CMD_GETTIMESTR
        sta     cmdreg
        jsr     mf_wait
        bcs     @nof
        ldy     #0
:       lda     paramreg
        and     #$7F
        sta     (ptr1),y
        iny
        cpy     #8
        bne     :-
        lda     #0
        sta     (ptr1),y
        lda     #1
        ldx     #0
        rts
@nof:   lda     #0
        tax
        rts

        .bss
tmp1:   .res    1
tmp2:   .res    1
