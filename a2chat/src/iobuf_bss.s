;
; One 1K ProDOS I/O buffer (FILEIO $BA00). Also eth_outp: files are closed
; during tcp_send so RX/TX are not the same RAM (send body Timeout).
;
        .export         iobuf_alloc, iobuf_free
        .export         eth_outp
        .import         incsp2, popptr1
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

iobuf_free:
        txa
        sec
        sbc     #>bufs
        lsr
        lsr
        tax
        cpx     #NBUFS
        bcs     :+
        lda     #$00
        sta     used,x
:       rts
