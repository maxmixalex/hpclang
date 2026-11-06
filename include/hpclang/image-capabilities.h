#ifndef HPCLANG_IMAGE_CAPABILITIES_H
#define HPCLANG_IMAGE_CAPABILITIES_H

/* Optional eight-byte prefix of the unpacked code payload, before _start.
 * The HPC1 file header is unchanged. Legacy payloads start at offset zero;
 * launchers inspect this prefix only when entry_offset is at least eight.
 * Unknown capabilities are ignored. Keep constants usable from assembly. */
#define HPCLANG_IMAGE_CAPABILITIES_MAGIC 0x58435048 /* "HPCX" */
#define HPCLANG_IMAGE_CAP_RPL_EXECUTION 0x00000001

#endif
