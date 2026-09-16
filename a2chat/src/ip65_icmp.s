;
; Drop ICMP so ping cannot copy eth_inp over FILEIO/eth_outp during TCP.
;
        .export         icmp_init
        .export         icmp_process

        .code

icmp_init:
        rts

icmp_process:
        sec
        rts
