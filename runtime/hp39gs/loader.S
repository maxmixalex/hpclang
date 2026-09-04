/*
 * Compatibility include for the original hp39gs clean-bootstrap path.
 *
 * The ARM-side M/R loader has now been demonstrated to consume the common
 * Launcher V3 ARMSYS register-block contract used across HP39, HP48gII and
 * HP49/50 launcher families. New code should reference
 * runtime/launcher-v3/mmu-loader.S directly.
 */
#include "../launcher-v3/mmu-loader.S"
