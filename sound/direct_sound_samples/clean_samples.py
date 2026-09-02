#!/usr/bin/env python3
"""
Clean all 115 samples - remove static, hum, normalize
PRESERVES smpl chunk/loop points
"""
import os
import struct

def read_smpl(data):
    """Parse smpl chunk data"""
    idx = data.find(b'smpl')
    if idx < 0:
        return None, None
    
    # Read chunk size
    size = struct.unpack('<I', data[idx+4:idx+8])[0]
    smpl_data = data[idx+8:idx+8+size]
    
    # Return smpl chunk (including header) and its position
    return data[idx:idx+8+size], idx

def clean_sample(filename):
    """Apply filters to clean audio, preserving smpl chunk"""
    print(f"Cleaning: {filename}")
    
    infile = filename
    temp = "cleaned/temp_" + filename
    outfile = "cleaned/" + filename
    
    # Create cleaned directory
    os.makedirs("cleaned", exist_ok=True)
    
    # Read original file and extract smpl chunk
    with open(infile, 'rb') as f:
        original = f.read()
    
    smpl_chunk, smpl_pos = read_smpl(original)
    
    if smpl_chunk:
        print(f"  Found smpl chunk at position {smpl_pos}")
    else:
        print(f"  No smpl chunk found")
    
    # Sox command: highpass (remove hum), lowpass (remove hiss), normalize
    # Output to temp file (no smpl chunk)
    cmd = f'sox "{infile}" "{temp}" highpass 30 lowpass 15000 norm -0.5'
    
    ret = os.system(cmd)
    if ret != 0:
        print(f"  -> Sox FAILED")
        return False
    
    # Read temp file
    with open(temp, 'rb') as f:
        cleaned = f.read()
    
    # If we had smpl chunk, re-insert it
    if smpl_chunk:
        # Find data chunk in cleaned file
        data_idx = cleaned.find(b'data')
        if data_idx > 0:
            # Insert smpl before data chunk
            new_data = cleaned[:data_idx] + smpl_chunk + cleaned[data_idx:]
            
            # Update RIFF size
            new_size = len(new_data) - 8
            new_data = new_data[:4] + struct.pack('<I', new_size) + new_data[8:]
            
            with open(outfile, 'wb') as f:
                f.write(new_data)
            print(f"  -> Cleaned + smpl preserved")
        else:
            # No data chunk found, just write cleaned
            with open(outfile, 'wb') as f:
                f.write(cleaned)
            print(f"  -> Cleaned (smpl not reinserted - no data chunk)")
    else:
        # No smpl chunk, just copy cleaned
        with open(outfile, 'wb') as f:
            f.write(cleaned)
        print(f"  -> Cleaned (no smpl)")
    
    # Remove temp
    os.remove(temp)
    return True

# All 115 files
files = [
    "bicycle_bell.wav", "classical_choir_voice_ahhs.wav", "dance_drums_ride_bell.wav",
    "drum_and_percussion_kick.wav", "ethnic_flavours_atarigane.wav",
    "ethnic_flavours_hyoushigi.wav", "ethnic_flavours_kotsuzumi.wav",
    "ethnic_flavours_ohtsuzumi.wav", "heart_of_asia_gamelan.wav", "register_noise.wav",
    "sc88pro_accordion.wav", "sc88pro_accordion_duplicate.wav", "sc88pro_bubbles.wav",
    "sc88pro_church_organ3_high.wav", "sc88pro_church_organ3_low.wav",
    "sc88pro_fingered_bass.wav", "sc88pro_flute.wav", "sc88pro_french_horn_60.wav",
    "sc88pro_french_horn_72.wav", "sc88pro_fretless_bass.wav", "sc88pro_glockenspiel.wav",
    "sc88pro_harp.wav", "sc88pro_jingle_bell.wav", "sc88pro_mute_high_conga.wav",
    "sc88pro_nylon_str_guitar.wav", "sc88pro_open_low_conga.wav",
    "sc88pro_orchestra_cymbal_crash.wav", "sc88pro_orchestra_snare.wav",
    "sc88pro_organ2.wav", "sc88pro_piano1_48.wav", "sc88pro_piano1_60.wav",
    "sc88pro_piano1_72.wav", "sc88pro_piano1_84.wav", "sc88pro_pizzicato_strings.wav",
    "sc88pro_rnd_kick.wav", "sc88pro_rnd_snare.wav", "sc88pro_slap_bass.wav",
    "sc88pro_square_wave.wav", "sc88pro_string_ensemble_60.wav",
    "sc88pro_string_ensemble_72.wav", "sc88pro_string_ensemble_84.wav",
    "sc88pro_synth_bass.wav", "sc88pro_taiko.wav", "sc88pro_tambourine.wav",
    "sc88pro_timpani.wav", "sc88pro_timpani_with_snare.wav", "sc88pro_tr909_hand_clap.wav",
    "sc88pro_trumpet_60.wav", "sc88pro_trumpet_72.wav", "sc88pro_trumpet_84.wav",
    "sc88pro_tuba_39.wav", "sc88pro_tuba_51.wav", "sc88pro_tubular_bell.wav",
    "sc88pro_wind.wav", "sc88pro_xylophone.wav", "sd90_ambient_tom.wav",
    "sd90_classical_detuned_ep1_high.wav", "sd90_classical_detuned_ep1_low.wav",
    "sd90_classical_distortion_guitar_high.wav", "sd90_classical_distortion_guitar_low.wav",
    "sd90_classical_oboe.wav", "sd90_classical_overdrive_guitar.wav",
    "sd90_classical_shakuhachi.wav", "sd90_classical_whistle.wav", "sd90_cowbell.wav",
    "sd90_enhanced_delay_shaku.wav", "sd90_open_triangle.wav", "sd90_solo_snare.wav",
    "sd90_special_scream_drive.wav", "steinway_b_piano.wav", "trinity_30303_mega_bass.wav",
    "trinity_big_boned.wav", "trinity_cymbal_crash.wav", "unknown_01.wav", "unknown_02.wav",
    "unknown_03.wav", "unknown_04.wav", "unknown_05.wav", "unknown_06.wav", "unknown_07.wav",
    "unknown_08.wav", "unknown_09.wav", "unknown_10.wav", "unknown_11.wav", "unknown_12.wav",
    "unknown_13.wav", "unknown_14.wav", "unknown_15.wav", "unknown_16.wav", "unknown_17.wav",
    "unknown_18.wav", "unknown_bell.wav", "unknown_close_hihat.wav", "unknown_female_voice.wav",
    "unknown_koto_high.wav", "unknown_koto_low.wav", "unknown_open_hihat.wav",
    "unknown_snare.wav", "unknown_synth_snare.wav",
    "unused_guitar_separates_power_chord.wav", "unused_heart_of_asia_indian_drum.wav",
    "unused_sc55_tom.wav", "unused_sc88pro_unison_slap.wav", "unused_sd90_oboe.wav",
    "unused_unknown_male_voice.wav"
]

print("Cleaning 115 samples (preserving smpl chunks)...")
success = 0
failed = 0

for f in files:
    if clean_sample(f):
        success += 1
    else:
        failed += 1

print(f"\n{'='*50}")
print(f"Complete: {success} cleaned, {failed} failed")
print("Copy to replace originals: cp cleaned/*.wav .")