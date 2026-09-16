;
; RX Ethernet frame in BSS (1024; TCP window 900). TX uses FILEIO (see iobuf_bss.s).
;
        .export         eth_inp

        .bss

eth_inp:
        .res            1024
