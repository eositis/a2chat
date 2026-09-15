;
; RX Ethernet frame in BSS. TX uses FILEIO (see iobuf_bss.s).
;
        .export         eth_inp

        .bss

eth_inp:
        .res            1518
