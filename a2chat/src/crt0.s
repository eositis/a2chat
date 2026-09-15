;
; cc65 apple2enh startup.
; Keep init in CODE, not ONCE. Do not jsr initlib. Do not load MAIN into
; $B800. Packet buffers stay in main RAM (LC $D400 is not writable here).
;
        .export         __STARTUP__ : absolute = 1
        .export         done, return
        .export         zpsave
        .export         _prodos_quit
        .import         zerobss, callmain
        .import         __dos_type
        .include        "zeropage.inc"
        .include        "apple2.inc"

        .segment        "STARTUP"

        ldx     #$FF
        txs
        sei
        cld
        lda     #$B1
        sta     $0400
        jsr     init
        jsr     zerobss
        jsr     setdos
        jmp     callmain

        .code

init:   ldx     #zpspace-1
:       lda     sp,x
        sta     zpsave,x
        dex
        bpl     :-

        lda     $BF00
        cmp     #$4C
        bne     basic
        lda     $BF6F
        cmp     #%00000001
        bne     basic
        lda     #<quit
        sta     done+1
        lda     #>quit
        sta     done+2
        lda     #<$BF00
        ldx     #>$BF00
        bne     setsp
basic:  lda     HIMEM
        ldx     HIMEM+1
setsp:  sta     sp
        stx     sp+1

        ldx     #<quit
        lda     #>quit
        jsr     reset

        ; Do not bit $C080 here. Bank 2 hides ProDOS in LC bank 1, so the
        ; first fopen/callmli (GET_FILE_INFO $C8) dies at callmli ($AC38).
        ; lc_map() is for IP65 eth buffers after file I/O.
        sei
        cld
        lda     #$B2
        sta     $0401
        lda     #<$BF00
        sta     sp
        lda     #>$BF00
        sta     sp+1
        sei
        cld
        lda     #$B3
        sta     $0402
        rts

; cc65 callmli no-ops when __dos_type is 0 (initdostype skipped).
setdos: lda     $BF00
        cmp     #$4C
        bne     @out
        lda     $BFFF
        cmp     #$10
        bcs     @st
        ora     #$10
@st:    sta     __dos_type
        inc     $BF94
@out:   rts

reset:  stx     SOFTEV
        sta     SOFTEV+1
        eor     #$A5
        sta     PWREDUP
return: rts

quit:
_prodos_quit:
        sei
        cld
        ldx     #zpspace-1
:       lda     zpsave,x
        sta     sp,x
        dex
        bpl     :-
        ; 40-col / main bank. Do not $C080/$C082: that unmaps ProDOS LC.
        sta     $C000
        sta     $C00C
        sta     $C051
        sta     $C054
        sta     $C002
        sta     $C004
        ldx     #$FF
        txs
        jsr     $BF00
        .byte   $65
        .word   q_param
        jmp     DOSWARM

        .rodata
q_param:.byte   $04
        .byte   $00
        .word   $0000
        .byte   $00
        .word   $0000

        .data
done:   jmp     DOSWARM

        .segment        "INIT"
zpsave: .res    zpspace
