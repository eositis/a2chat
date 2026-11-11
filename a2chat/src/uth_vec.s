; Uthernet II overlay header. Must be the first CODE in the link.
; Offsets match struct nic in src/nic.h.
;
        .import _uth_probe
        .import _uth_init
        .import _uth_connect
        .import _uth_send
        .import _uth_recv
        .import _uth_connected
        .import _uth_disconnect
        .import _uth_localtime
        .import _uth_last_error
        .import _uth_strerror
        .import _uth_should_retry
        .import _uth_report

        .segment "CODE"
        .export uth_image
uth_image:
        .byte "NIC1"
        .byte $00               ; flags
        .byte 1                 ; STREAM default: word by word
        .word 0                 ; load_size, patched by tools/mknic.py
        .addr _uth_probe
        .addr _uth_init
        .addr _uth_connect
        .addr _uth_send
        .addr _uth_recv
        .addr _uth_connected
        .addr _uth_disconnect
        .addr _uth_localtime
        .addr _uth_last_error
        .addr _uth_strerror
        .addr _uth_should_retry
        .addr _uth_report
        .word 0                 ; no wall clock
        .word 0                 ; no clock name
