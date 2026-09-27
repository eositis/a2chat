;
; One 1K ProDOS I/O buffer (FILEIO $BA00). Also eth_outp: files are closed
; during tcp_send so RX/TX are not the same RAM (send body Timeout).
;
        .export         iobuf_alloc, iobuf_free
        .export         _iobuf_reclaim
        .export         eth_outp
        .import         incsp2, popptr1, fdtab, closedirect
        .include        "zeropage.inc"
        .include        "errno.inc"

NBUFS   = 1

.segment "FILEIO"
bufs:
eth_outp:
        .res            NBUFS * $0400

.bss
used:   .res            NBUFS

.code

iobuf_alloc:
        jsr     incsp2
        jsr     popptr1
        ldx     #$00
:       lda     used,x
        beq     found
        inx
        cpx     #NBUFS
        bcc     :-
        lda     #ENOMEM
        rts

found:  lda     #$FF
        sta     used,x
        txa
        asl
        asl
        clc
        adc     #>bufs
        ldy     #$01
        sta     (ptr1),y
        dey
        tya
        sta     (ptr1),y
        ldx     #$00
        rts

; NBUFS=1. A failed OPEN calls here with a bad pointer (MLI
; clobbers the fd slot index), which used to skip the release
; and leave `used` set. Every later fopen then returns ENOMEM.
iobuf_free:
        lda     #$00
        sta     used
        rts

; Drop any real ProDOS file (fd 3+) and clear the buffer flag.
; The flag can stay set after a failed open, and then every fopen
; returns ENOMEM before MLI runs.
_iobuf_reclaim:
        ldy     #12
@lp:    lda     fdtab,y
        beq     @nx
        cmp     #$80
        bcs     @nx
        jsr     closedirect
        lda     #$00
        sta     fdtab,y
@nx:    tya
        clc
        adc     #4
        tay
        cpy     #32
        bcc     @lp
        lda     #$00
        sta     used
        rts
