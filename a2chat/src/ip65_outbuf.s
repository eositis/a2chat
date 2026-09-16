;
; DHCP scratch. Do not alias eth_outp (udp_send overlapping copy).
;
        .export         output_buffer

        .bss

output_buffer:
        .res            300
