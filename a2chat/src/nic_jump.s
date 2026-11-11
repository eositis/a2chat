; Indirect calls through struct nic at _nic_hole. Offsets match src/nic.h.
; The caller removes stack arguments, so a failed call just returns.
; cc65 passes the last argument in A/X. Save both around the magic check.
; 65C02 jmp (abs) has no page-boundary bug.

        .import _nic_hole
        .segment "CODE"

live:
        lda _nic_hole
        cmp #'N'
        bne @out
        lda _nic_hole+1
        cmp #'I'
        bne @out
        lda _nic_hole+2
        cmp #'C'
        bne @out
        lda _nic_hole+3
        cmp #'1'
@out:   rts

.macro niccall off
        pha
        txa
        pha
        jsr live
        bne @dead
        lda _nic_hole+off
        ora _nic_hole+off+1
        beq @dead
        pla
        tax
        pla
        jmp (_nic_hole+off)
@dead:  pla
        pla
.endmacro

.proc _nic_connect
        niccall 12
        lda #$FF
        tax
        rts
.endproc
.export _nic_connect

.proc _nic_send
        niccall 14
        lda #$FF
        tax
        rts
.endproc
.export _nic_send

.proc _nic_recv
        niccall 16
        lda #0
        tax
        rts
.endproc
.export _nic_recv

.proc _nic_connected
        niccall 18
        lda #0
        tax
        rts
.endproc
.export _nic_connected

.proc _nic_disconnect
        niccall 20
        rts
.endproc
.export _nic_disconnect

.proc _nic_localtime
        niccall 22
        lda #$FF
        tax
        rts
.endproc
.export _nic_localtime

.proc _nic_last_error
        niccall 24
        lda #0
        tax
        rts
.endproc
.export _nic_last_error

.proc _nic_strerror
        niccall 26
        lda #0
        tax
        rts
.endproc
.export _nic_strerror

.proc _nic_should_retry
        niccall 28
        lda #1
        ldx #0
        rts
.endproc
.export _nic_should_retry

.proc _nic_report
        niccall 30
        rts
.endproc
.export _nic_report

.proc _nic_wall
        niccall 32
        rts
.endproc
.export _nic_wall

.proc _nic_flags
        jsr live
        bne @no
        lda _nic_hole+4
        ldx #0
        rts
@no:    lda #0
        tax
        rts
.endproc
.export _nic_flags

.proc _nic_stream_default
        jsr live
        bne @no
        lda _nic_hole+5
        ldx #0
        rts
@no:    lda #1
        ldx #0
        rts
.endproc
.export _nic_stream_default

.proc _nic_has_clock
        jsr live
        bne @no
        lda _nic_hole+4
        and #$04
        beq @no
        lda _nic_hole+32
        ora _nic_hole+33
        beq @no
        lda #1
        ldx #0
        rts
@no:    lda #0
        tax
        rts
.endproc
.export _nic_has_clock

.proc _nic_clock_name
        jsr live
        bne @no
        lda _nic_hole+34
        ldx _nic_hole+35
        rts
@no:    lda #0
        tax
        rts
.endproc
.export _nic_clock_name
