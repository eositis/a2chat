;
; A2CHAT timer_read: no WAIT, no $C080/$C082.
; MegaFlash ms if IIc+MF (same probe as mfclock.s); else VBL edges (~16 ms).
;
        .export         timer_init
        .export         timer_read
        .export         timer_seconds
        .import         _mf_present
        .import         _mf_get_ms
        .import         _mf_reset_ms

RDVBLBAR        = $C019
MACHID          = $FBB3
SLOTWID         = $FBC0

        .bss
current_time:   .res    2
last_vbl:       .res    1
use_mf:         .res    1

        .code

timer_init:
        lda     #0
        sta     current_time
        sta     current_time+1
        sta     use_mf
        lda     RDVBLBAR
        sta     last_vbl
        lda     MACHID
        cmp     #6
        bne     @done
        lda     SLOTWID
        bne     @done
        jsr     _mf_present
        cmp     #1
        bne     @done
        sta     use_mf
        jsr     _mf_reset_ms
@done:  rts

timer_read:
        lda     use_mf
        beq     @vbl
        jmp     _mf_get_ms

@vbl:   lda     RDVBLBAR
        tax
        eor     last_vbl
        stx     last_vbl
        bpl     @ret
        txa
        bmi     @ret
        lda     current_time
        clc
        adc     #16
        sta     current_time
        bcc     @ret
        inc     current_time+1
@ret:   lda     current_time
        ldx     current_time+1
        rts

timer_seconds:
        lda     #0
        rts
