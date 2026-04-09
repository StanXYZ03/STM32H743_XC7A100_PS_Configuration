/*
 * Placeholder file kept in the project on purpose.
 *
 * We no longer override ARMCC stdio syscalls here because the full C library
 * already provides sys_io/stdio_streams objects and overriding them caused
 * multiply-defined symbol errors.
 */