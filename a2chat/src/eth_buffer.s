;
; RX Ethernet frame in BSS (960; TCP window 900, frame about 954). TX uses FILEIO.
;
        .export         eth_inp
        .export         _dirblk
_dirblk         = eth_inp

        .bss

eth_inp:
        .res            960
