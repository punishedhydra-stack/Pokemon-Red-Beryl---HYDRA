#ifndef GUARD_HYDRA_RADIO_H
#define GUARD_HYDRA_RADIO_H

// HYDRA radio secret base decoration.
// HydraRadio_OpenMenu: special opened from the radio's interaction script; a sound-test-style
//   track browser (left/right pick the digit, up/down change the track, A plays, START stops,
//   B saves + closes). The chosen track is stored in VAR_HYDRA_RADIO_TRACK.
// HydraRadio_PlayOnEnter: (re)applies the saved track in the player's own base; safe to call from
//   the map-resume / return-to-field hooks (idempotent).
void HydraRadio_OpenMenu(void);
void HydraRadio_PlayOnEnter(void);
u16 HydraRadio_GetBaseMusicOverride(void); // MUS_DUMMY unless in the player's own base w/ a radio

#endif // GUARD_HYDRA_RADIO_H
