; CoGS overlay header. Must be the first CODE in the link.
; Offsets match struct nic in src/nic.h.
; Flags: hostname 1, ignore IP= 2, clock 4.
;
        .import _cog_probe
        .import _cog_init
        .import _cog_connect
        .import _cog_send
        .import _cog_recv
        .import _cog_connected
        .import _cog_disconnect
        .import _cog_localtime
        .import _cog_last_error
        .import _cog_strerror
        .import _cog_should_retry
        .import _cog_report
        .import _cog_wall

        .segment "RODATA"
clock_name:
        .byte "COGS", 0

        .segment "CODE"
        .export cog_image
cog_image:
        .byte "NIC1"
        .byte $07
        .byte 0                 ; STREAM default: whole reply
        .word 0
        .addr _cog_probe
        .addr _cog_init
        .addr _cog_connect
        .addr _cog_send
        .addr _cog_recv
        .addr _cog_connected
        .addr _cog_disconnect
        .addr _cog_localtime
        .addr _cog_last_error
        .addr _cog_strerror
        .addr _cog_should_retry
        .addr _cog_report
        .addr _cog_wall
        .addr clock_name
