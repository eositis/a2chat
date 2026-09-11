;
; Aux-bank block copy for 128K IIe/IIc.
; RAMRD affects instruction fetch in $0200-$BFFF, so this code lives in LC.
; 80-col firmware owns aux $0400-$07FF; we use $4000-$7FFF (hires page 2).
;
        .export         _aux_present
        .export         _aux_write
        .export         _aux_read
        .import         popax
        .include        "zeropage.inc"

AUX_BASE        = $4000
; IIe/IIc: $C002/$C003 = RAMRD (read main/aux), $C004/$C005 = RAMWRT (write main/aux).
CLR_RAMRD       = $C002
SET_RAMRD       = $C003
CLR_RAMWRT      = $C004
SET_RAMWRT      = $C005

        .segment        "LC"

; unsigned char aux_present(void)
_aux_present:
        php
        sei
        lda     $4000
        sta     tmp1
        sta     SET_RAMWRT
        eor     #$FF
        sta     $4000
        sta     CLR_RAMWRT
        lda     $4000
        cmp     tmp1
        bne     @no              ; main changed: 64K or RAMWRT ignored
        sta     SET_RAMRD
        lda     $4000
        sta     CLR_RAMRD
        eor     tmp1
        cmp     #$FF
        bne     @no
        lda     #1
        ldx     #0
        plp
        rts
@no:    lda     tmp1
        sta     $4000
        lda     #0
        tax
        plp
        rts

; void __fastcall__ aux_write(unsigned off, const unsigned char *src, unsigned n)
_aux_write:
        sta     tmp1
        stx     tmp2
        jsr     popax
        sta     ptr1
        stx     ptr1+1
        jsr     popax
        jsr     set_auxptr
        lda     tmp1
        ora     tmp2
        beq     @out
        php
        sei
        sta     CLR_RAMRD
        sta     SET_RAMWRT
        jsr     copy_to_aux
        sta     CLR_RAMWRT
        plp
@out:   rts

; void __fastcall__ aux_read(unsigned off, unsigned char *dst, unsigned n)
_aux_read:
        sta     tmp1
        stx     tmp2
        jsr     popax
        sta     ptr1
        stx     ptr1+1
        jsr     popax
        jsr     set_auxptr
        lda     tmp1
        ora     tmp2
        beq     @out
        php
        sei
        sta     CLR_RAMWRT
        sta     SET_RAMRD
        jsr     copy_from_aux
        sta     CLR_RAMRD
        plp
@out:   rts

set_auxptr:
        clc
        adc     #<AUX_BASE
        sta     ptr2
        txa
        adc     #>AUX_BASE
        sta     ptr2+1
        rts

copy_to_aux:
        ldy     #0
        ldx     tmp2
        beq     @part
@page:  lda     (ptr1),y
        sta     (ptr2),y
        iny
        bne     @page
        inc     ptr1+1
        inc     ptr2+1
        dex
        bne     @page
@part:  lda     tmp1
        beq     @done
@pl:    lda     (ptr1),y
        sta     (ptr2),y
        iny
        cpy     tmp1
        bne     @pl
@done:  rts

copy_from_aux:
        ldy     #0
        ldx     tmp2
        beq     @part
@page:  lda     (ptr2),y
        sta     (ptr1),y
        iny
        bne     @page
        inc     ptr1+1
        inc     ptr2+1
        dex
        bne     @page
@part:  lda     tmp1
        beq     @done
@pl:    lda     (ptr2),y
        sta     (ptr1),y
        iny
        cpy     tmp1
        bne     @pl
@done:  rts
