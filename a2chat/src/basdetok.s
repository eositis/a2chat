;
; List a tokenized Applesoft image to text.
; Image is already in aux at $4000 (length in AX, max $4000).
; Text lands in aux at $8000. Keyword names are copied from the
; ROM table at $D0D0 into eth_outp while no file is open.
; Returns the text length, or 0 if the image is not tokenized
; or the ROM table is not END.
;
        .include        "ovlsym.inc"

        .export         run

CLR_RAMRD       = $C002
SET_RAMRD       = $C003
CLR_RAMWRT      = $C004
SET_RAMWRT      = $C005
SRC             = $4000
DST             = $8000
TAB             = $D0D0
TABLIM          = eth_outp + $200

        .code

; Called at eth_inp. AX = ProDOS path. A = 1 if A2CHAT.LST was written.
run:    sta     ppath
        stx     ppath+1
        jsr     readaux
        cmp     #6
        bcs     @ge
        cpx     #0
        beq     fail
@ge:    jsr     listimg
        cmp     #0
        bne     @wr
        cpx     #0
        beq     fail
@wr:    sta     nbytes
        stx     nbytes+1
        jmp     writelst

fail:   lda     #0
        tax
        rts

readaux:
        lda     ppath
        ldx     ppath+1
        jsr     pushax
        lda     #<m_rb
        ldx     #>m_rb
        jsr     _fopen
        sta     fhand
        stx     fhand+1
        ora     fhand+1
        bne     @op
        lda     #0
        tax
        rts
@op:    lda     #$00
        sta     dest
        lda     #$40
        sta     dest+1
@rd:    lda     fhand
        ldx     fhand+1
        jsr     _fgetc
        cpx     #$FF
        beq     @eof
        sta     onech
        lda     dest
        sta     ptr2
        lda     dest+1
        sta     ptr2+1
        lda     onech
        jsr     aput
        inc     dest
        bne     @cap
        inc     dest+1
@cap:   lda     dest+1
        cmp     #$80
        bcc     @rd
@eof:   lda     fhand
        ldx     fhand+1
        jsr     _fclose
        lda     dest
        pha
        lda     dest+1
        sec
        sbc     #$40
        tax
        pla
        rts

aput:   php
        sei
        ldy     #0
        sta     SET_RAMWRT
        sta     (ptr2),y
        sta     CLR_RAMWRT
        plp
        rts

writelst:
        lda     #4
        sta     __filetype
        lda     #0
        sta     __auxtype
        sta     __auxtype+1
        lda     #<m_lst
        ldx     #>m_lst
        jsr     pushax
        lda     #<m_wb
        ldx     #>m_wb
        jsr     _fopen
        sta     fhand
        stx     fhand+1
        ora     fhand+1
        bne     @op
        jmp     fail
@op:    lda     #$00
        sta     dest
        lda     #$80
        sta     dest+1
@wr:    lda     nbytes
        ora     nbytes+1
        beq     @cl
        jsr     aget
        ldx     #0
        jsr     pushax
        lda     fhand
        ldx     fhand+1
        jsr     _fputc
        inc     dest
        bne     @dec
        inc     dest+1
@dec:   lda     nbytes
        bne     @dl
        dec     nbytes+1
@dl:    dec     nbytes
        jmp     @wr
@cl:    lda     fhand
        ldx     fhand+1
        jsr     _fclose
        ; File type is already TXT via __filetype. p8_set_txt saves
        ; zero page inside eth_inp, which is this overlay.
        lda     #1
        ldx     #0
        rts

aget:   jsr     stub_in
        lda     dest
        sta     ptr1
        lda     dest+1
        sta     ptr1+1
        jsr     $50
        sta     onech
        jsr     stub_out
        lda     onech
        rts

listimg:
        sta     ptr3
        txa
        clc
        adc     #>SRC
        sta     ptr3+1
        cld
        jsr     stub_in

        lda     #<SRC
        sta     ptr1
        lda     #>SRC
        sta     ptr1+1
        ldx     #0
@hd:    jsr     $50
        sta     eth_outp,x
        inc     ptr1
        bne     @hdi
        inc     ptr1+1
@hdi:   inx
        cpx     #4
        bne     @hd

        lda     eth_outp+1
        beq     @bad
        cmp     #$97
        bcs     @bad
        lda     eth_outp
        cmp     #'0'
        bcc     @nd
        cmp     #$3A
        bcc     @bad
@nd:    lda     eth_outp+3
        cmp     #$FA
        bcs     @bad
        jsr     rom_tab
        lda     eth_outp
        cmp     #$45
        beq     @tok
@bad:   jmp     bad
@tok:

        lda     #<SRC
        sta     ptr1
        lda     #>SRC
        sta     ptr1+1
        lda     #<DST
        sta     ptr2
        lda     #>DST
        sta     ptr2+1

lin:    jsr     getb
        bcc     @n0
        jmp     fin
@n0:    sta     tmp4
        jsr     getb
        bcc     @n1
        jmp     fin
@n1:    ora     tmp4
        bne     @n2
        jmp     fin
@n2:    jsr     getb
        bcc     @n3
        jmp     fin
@n3:    sta     tmp4
        jsr     getb
        bcc     @n4
        jmp     fin
@n4:    tax
        lda     tmp4
        jsr     putnum
        lda     #' '
        jsr     emit
        lda     #0
        sta     tmp1
body:   jsr     getb
        bcc     @n5
        jmp     fin
@n5:
        cmp     #0
        beq     eol
        bit     tmp1
        bvs     lit
        bmi     inq
        cmp     #$22
        beq     doq
        cmp     #$80
        bcc     lit
        cmp     #$EB
        bcs     qmark
        cmp     #$B2
        bne     exp
        lda     tmp1
        ora     #$40
        sta     tmp1
        lda     #$B2
exp:    jsr     expand
        jmp     body
qmark:  lda     #'?'
        jsr     emit
        jmp     body
doq:    lda     tmp1
        eor     #$80
        sta     tmp1
        lda     #$22
        jsr     emit
        jmp     body
inq:    cmp     #$22
        bne     lit
        lda     tmp1
        eor     #$80
        sta     tmp1
        lda     #$22
        jsr     emit
        jmp     body
lit:    and     #$7F
        beq     body
        jsr     emit
        jmp     body
eol:    lda     #$0D
        jsr     emit
        jmp     lin

fin:    lda     ptr2
        pha
        lda     ptr2+1
        sec
        sbc     #>DST
        pha
        jsr     stub_out
        pla
        tax
        pla
        rts

bad:    jsr     stub_out
        lda     #0
        tax
        rts

; A = char. Drop once the listing reaches aux $C000.
emit:   ldx     ptr2+1
        cpx     #$C0
        bcs     @x
        php
        sei
        sta     SET_RAMWRT
        ldy     #0
        sta     (ptr2),y
        sta     CLR_RAMWRT
        plp
        inc     ptr2
        bne     @x
        inc     ptr2+1
@x:     rts

; Byte at ptr1 into A. Carry set when ptr1 >= ptr3.
getb:   lda     ptr1
        cmp     ptr3
        lda     ptr1+1
        sbc     ptr3+1
        bcs     @eof
        jsr     $50
        inc     ptr1
        bne     @ok
        inc     ptr1+1
@ok:    clc
        rts
@eof:   sec
        rts

; AX = line number.
putnum: sta     tmp2
        stx     tmp3
        lda     #0
        sta     tmp1
        ldy     #0
@p:     ldx     #0
        lda     tens,y
        sta     ptr4
        lda     tens+1,y
        sta     ptr4+1
@s:     lda     tmp2
        sec
        sbc     ptr4
        sta     tmp4
        lda     tmp3
        sbc     ptr4+1
        bcc     @e
        sta     tmp3
        lda     tmp4
        sta     tmp2
        inx
        bne     @s
@e:     txa
        bne     @dig
        lda     tmp1
        beq     @nx
@dig:   inc     tmp1
        txa
        ora     #$30
        sta     tmp4
        tya
        pha
        lda     tmp4
        jsr     emit
        pla
        tay
@nx:    iny
        iny
        cpy     #10
        bne     @p
        lda     tmp1
        bne     @z
        lda     #'0'
        jsr     emit
@z:     rts

; A = token $80-$EA. Names in eth_outp, high bit ends each word.
expand: sec
        sbc     #$80
        tax
        lda     #<eth_outp
        sta     ptr4
        lda     #>eth_outp
        sta     ptr4+1
        ldy     #0
@sk:    cpx     #0
        beq     @cp
@ch:    lda     ptr4+1
        cmp     #>TABLIM
        bcs     @bad
        lda     (ptr4),y
        pha
        jsr     bump4
        pla
        bpl     @ch
        dex
        jmp     @sk
@cp:    lda     ptr4+1
        cmp     #>TABLIM
        bcs     @bad
        lda     (ptr4),y
        pha
        and     #$7F
        sty     tmp4
        jsr     emit
        ldy     tmp4
        jsr     bump4
        pla
        bpl     @cp
        rts
@bad:   lda     #'?'
        jmp     emit

bump4:  iny
        bne     @r
        inc     ptr4+1
        ldy     #0
@r:     rts

; Two pages from $D0D0. ProDOS stays in LC bank 1; restore it after.
rom_tab:
        lda     #<(TAB)
        sta     ptr1
        lda     #>(TAB)
        sta     ptr1+1
        lda     #<eth_outp
        sta     ptr2
        lda     #>eth_outp
        sta     ptr2+1
        php
        sei
        sta     CLR_RAMRD
        sta     CLR_RAMWRT
        bit     $C082
        ldy     #0
        ldx     #2
@pg:    lda     (ptr1),y
        sta     (ptr2),y
        iny
        bne     @pg
        inc     ptr1+1
        inc     ptr2+1
        dex
        bne     @pg
        bit     $C08B
        bit     $C08B
        plp
        rts

stub_in:
        ldx     #stub_len-1
@sv:    lda     $50,x
        sta     $0300,x
        dex
        bpl     @sv
        ldx     #0
@cp:    lda     stub,x
        sta     $50,x
        inx
        cpx     #stub_len
        bne     @cp
        rts

stub_out:
        ldx     #stub_len-1
@rs:    lda     $0300,x
        sta     $50,x
        dex
        bpl     @rs
        rts

        .rodata
tens:   .word   10000, 1000, 100, 10, 1
m_rb:   .byte   "rb", 0
m_wb:   .byte   "wb", 0
m_lst:  .byte   "A2CHAT.LST", 0
stub:   php
        sei
        sta     SET_RAMRD
        ldy     #0
        lda     (ptr1),y
        sta     CLR_RAMRD
        plp
        rts
stub_len = * - stub

        .bss
ppath:  .res    2
fhand:  .res    2
nbytes: .res    2
dest:   .res    2
onech:  .res    1
