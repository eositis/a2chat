;
; Page-aligned ProDOS I/O buffers (linker FILEIO at $B400).
; Default cc65 iobuf uses posix_memalign on the heap; after linking
; ip65_tcp.lib the heap cannot hold two 1K buffers (PAY + BOD).
;
        .export         iobuf_alloc, iobuf_free
        .import         incsp2, popptr1
        .include        "zeropage.inc"
        .include        "errno.inc"

NBUFS   = 2

.segment "FILEIO"
bufs:   .res            NBUFS * $0400

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
