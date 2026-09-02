#ifndef GUARD_CONFIG_LINK_H
#define GUARD_CONFIG_LINK_H

// HYDRA: Master switch to gut all cable + wireless multiplayer to reclaim EWRAM.
// When TRUE, the link / RFU / union-room / trade / mystery-gift / record-mixing
// modules are compiled out and their large EWRAM buffers (gLink ~4KB, gRfu ~3.4KB,
// gBlockRecvBuffer, gBlockSendBuffer, etc.) are removed. src/link_stubs.c provides
// the minimal residual symbols the surviving single-player battle engine references.
// Kept FALSE for Stage 0 (no behavior change); flipped TRUE once the cuts are wired.
#define FREE_LINK   TRUE    //HYDRA

#endif // GUARD_CONFIG_LINK_H
