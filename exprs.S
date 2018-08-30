        .section .data,"aw"
.default_exprs_data_start:
        .incbin "escape_exprs"
        .byte  0
.default_exprs_data_end:
        .global default_exprs
        .section .rodata,"a"
        .size default_exprs, .default_exprs_end - default_exprs
default_exprs:
        .quad  .default_exprs_data_start
.default_exprs_end:
        .global default_exprs_size
        .section .rodata,"a"
        .size default_exprs_size, .default_exprs_size_end - default_exprs_size
default_exprs_size:
        .quad .default_exprs_data_end - .default_exprs_data_start
.default_exprs_size_end:
