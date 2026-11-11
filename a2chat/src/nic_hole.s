; Resident hole for one NIC overlay. The memory area is NIC_HOLE_SIZE
; bytes ending at FILEIO ($BA00). Only the label is stored here; the
; rest of the area is reserved by cfg/apple2enh-a2chat.cfg and filled
; from disk after the card is identified.
;
; _nic_hole is the NIC area start, exported by the linker config.
; CFGINIT occupies that address until the overlay is loaded, so this
; segment must not claim the first byte.
        .segment "NICHOLE"
