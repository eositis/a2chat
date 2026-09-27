;
; DHCP scratch. Do not alias eth_outp (udp_send overlapping copy).
;
        .export         output_buffer
        .export         _output_buffer

        .bss

output_buffer:
_output_buffer  = output_buffer
        .res            256
