; ICMP echo with caller-supplied TTL for traceroute (IP65 helper)

.include "zeropage.inc"
.include "common.inc"
.include "error.inc"

.export _icmp_trace_dest
.export _icmp_trace_from
.export _icmp_trace_ms
.export _icmp_trace_ttl
.export _icmp_trace_hop

.import icmp_echo_ip
.import icmp_add_listener
.import icmp_remove_listener
.import icmp_callback
.import icmp_inp
.import ip_outp
.import ip_inp
.import ip_create_packet
.import ip_send
.import ip_calc_cksum
.import ip65_process
.import ip65_error
.import timer_read
.import timer_timeout
.import check_for_abort_key

.importzp ip_dest
.importzp ip_src
.importzp ip_len
.importzp ip_ttl
.importzp ip_proto
.importzp ip_proto_icmp
.importzp ip_cksum_ptr
.importzp ip_data

icmp_outp = ip_outp + ip_data
icmp_type = 0
icmp_code = 1
icmp_cksum = 2
icmp_echo_id = 4
icmp_echo_seq = 6
icmp_echo_data = 8

icmp_echo_request = 8
icmp_echo_reply = 0
icmp_time_exceeded = 11


.bss

_icmp_trace_dest: .res 4
_icmp_trace_from: .res 4
_icmp_trace_ms:   .res 2
_icmp_trace_ttl:  .res 1

wait_ms:    .res 2
deadline:   .res 2
start_t:    .res 2
seq:        .res 2
state:      .res 1
arp_timer:  .res 2


.code

_icmp_trace_hop:
  stax wait_ms
  ldx #3
: lda _icmp_trace_dest,x
  sta icmp_echo_ip,x
  dex
  bpl :-

  lda #0
  sta state
  sta _icmp_trace_from
  sta _icmp_trace_from+1
  sta _icmp_trace_from+2
  sta _icmp_trace_from+3
  sta _icmp_trace_ms
  sta _icmp_trace_ms+1

  jsr send_echo_ttl
  bcc sent_ok

  jsr timer_read
  stax arp_timer
arp_wait:
  jsr ip65_process
  jsr check_for_abort_key
  bcc :+
  jmp abort_err
:
  ldax arp_timer
  clc
  adc #50
  bcc :+
  inx
: jsr timer_timeout
  bcs arp_wait
  jsr send_echo_ttl
  bcc sent_ok
  lda #IP65_ERROR_TRANSMIT_FAILED
  sta ip65_error
  lda #0
  ldx #0
  rts

sent_ok:
  jsr timer_read
  stax start_t
  clc
  adc wait_ms
  sta deadline
  txa
  adc wait_ms+1
  sta deadline+1

  ldax #cb_exceeded
  stax icmp_callback
  lda #icmp_time_exceeded
  jsr icmp_add_listener

  ldax #cb_reply
  stax icmp_callback
  lda #icmp_echo_reply
  jsr icmp_add_listener

poll:
  jsr ip65_process
  jsr check_for_abort_key
  bcc :+
  jmp abort_clean
:
  lda state
  bne got
  ldax deadline
  jsr timer_timeout
  bcs poll

  jsr drop_listeners
  lda #IP65_ERROR_TIMEOUT_ON_RECEIVE
  sta ip65_error
  lda #0
  ldx #0
  rts

got:
  jsr drop_listeners
  jsr timer_read
  sec
  sbc start_t
  sta _icmp_trace_ms
  txa
  sbc start_t+1
  sta _icmp_trace_ms+1
  lda state
  ldx #0
  rts

abort_clean:
  jsr drop_listeners
abort_err:
  lda #IP65_ERROR_ABORTED_BY_USER
  sta ip65_error
  lda #0
  ldx #0
  rts

drop_listeners:
  lda #icmp_time_exceeded
  jsr icmp_remove_listener
  lda #icmp_echo_reply
  jmp icmp_remove_listener

copy_src:
  ldx #3
: lda ip_inp + ip_src,x
  sta _icmp_trace_from,x
  dex
  bpl :-
  rts

cb_exceeded:
  lda #1
  sta state
  jmp copy_src

cb_reply:
  lda #2
  sta state
  jmp copy_src

send_echo_ttl:
  ldy #3
: lda icmp_echo_ip,y
  sta ip_outp + ip_dest,y
  dey
  bpl :-

  lda #icmp_echo_request
  sta icmp_outp + icmp_type
  lda #0
  sta icmp_outp + icmp_code
  sta icmp_outp + icmp_cksum
  sta icmp_outp + icmp_cksum + 1
  sta icmp_outp + icmp_echo_id
  sta icmp_outp + icmp_echo_id + 1
  inc seq
  bne :+
  inc seq+1
: ldax seq
  stax icmp_outp + icmp_echo_seq

  ldy #0
: lda payload,y
  beq @len
  sta icmp_outp + icmp_echo_data,y
  iny
  bne :-
@len:
  tya
  clc
  adc #28
  sta ip_outp + ip_len + 1
  lda #0
  sta ip_outp + ip_len

  ldax #icmp_outp
  stax ip_cksum_ptr
  tya
  clc
  adc #8
  ldx #0
  jsr ip_calc_cksum
  stax icmp_outp + icmp_cksum
  lda #ip_proto_icmp
  sta ip_outp + ip_proto
  jsr ip_create_packet
  lda _icmp_trace_ttl
  sta ip_outp + ip_ttl
  jmp ip_send

.rodata

payload:
  .byte "A2NETTOOL",0
