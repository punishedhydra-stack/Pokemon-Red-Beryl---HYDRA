#!/usr/bin/env python3
import wave
import struct
import os

TARGET_RATE = 31536

def read_smpl(data):
    """Parse smpl chunk data"""
    if len(data) < 36:
        return None
    
    # Unpack header
    vals = struct.unpack('<9I', data[:36])
    num_loops = vals[7]
    
    loops = []
    pos = 36
    for i in range(num_loops):
        if pos + 24 > len(data):
            break
        loop_vals = struct.unpack('<6I', data[pos:pos+24])
        loops.append({
            'id': loop_vals[0],
            'type': loop_vals[1],
            'start': loop_vals[2],
            'end': loop_vals[3],
            'fraction': loop_vals[4],
            'play_count': loop_vals[5]
        })
        pos += 24
    
    return {
        'manufacturer': vals[0],
        'product': vals[1],
        'sample_period': vals[2],
        'midi_note': vals[3],
        'midi_pitch': vals[4],
        'smpte_format': vals[5],
        'smpte_offset': vals[6],
        'loops': loops,
        'sampler_data': vals[8]
    }

def write_smpl(smpl):
    """Create smpl chunk bytes"""
    header = struct.pack('<9I',
        smpl['manufacturer'],
        smpl['product'],
        smpl['sample_period'],
        smpl['midi_note'],
        smpl['midi_pitch'],
        smpl['smpte_format'],
        smpl['smpte_offset'],
        len(smpl['loops']),
        smpl['sampler_data']
    )
    
    loops = b''
    for loop in smpl['loops']:
        loops += struct.pack('<6I',
            loop['id'], loop['type'], loop['start'],
            loop['end'], loop['fraction'], loop['play_count']
        )
    
    return header + loops

def convert_file(filename, orig_rate, factor):
    print(f"\n{filename}: {orig_rate}Hz -> {TARGET_RATE}Hz (factor: {factor})")
    
    # Read smpl chunk from original
    smpl = None
    with open(filename, 'rb') as f:
        data = f.read()
        
        # Find smpl chunk
        idx = data.find(b'smpl')
        if idx > 0:
            # Read chunk size (4 bytes before data)
            size = struct.unpack('<I', data[idx+4:idx+8])[0]
            smpl = read_smpl(data[idx+8:idx+8+size])
            print(f"  Found smpl with {len(smpl['loops'])} loop(s)")
    
    # Scale loop points
    if smpl and smpl['loops']:
        for i, loop in enumerate(smpl['loops']):
            old_s = loop['start']
            old_e = loop['end']
            loop['start'] = int(loop['start'] * factor)
            loop['end'] = int(loop['end'] * factor)
            print(f"    Loop {i}: {old_s}-{old_e} -> {loop['start']}-{loop['end']}")
        
        # Update sample period for new rate
        smpl['sample_period'] = int(1000000000 / TARGET_RATE)
    
    # Convert audio with sox
    temp = filename + '.tmp.wav'
    os.system(f'sox "{filename}" -r {TARGET_RATE} "{temp}" rate -h')
    
    # Read converted audio
    with wave.open(temp, 'rb') as w:
        audio = w.readframes(w.getnframes())
        params = (w.getnchannels(), w.getsampwidth(), w.getframerate())
    
    # Write output with smpl chunk
    out = 'converted/' + filename
    os.makedirs('converted', exist_ok=True)
    
    with wave.open(out, 'wb') as w:
        w.setnchannels(params[0])
        w.setsampwidth(params[1])
        w.setframerate(params[2])
        w.writeframes(audio)
    
    # Add smpl chunk back
    if smpl and smpl['loops']:
        with open(out, 'rb') as f:
            data = bytearray(f.read())
        
        # Find data chunk and insert smpl before it
        data_idx = data.find(b'data')
        if data_idx > 0:
            smpl_bytes = write_smpl(smpl)
            smpl_chunk = b'smpl' + struct.pack('<I', len(smpl_bytes)) + smpl_bytes
            
            new_data = data[:data_idx] + smpl_chunk + data[data_idx:]
            # Update RIFF size
            new_data[4:8] = struct.pack('<I', len(new_data) - 8)
            
            with open(out, 'wb') as f:
                f.write(new_data)
            print(f"  Written scaled loops")
    
    os.remove(temp)
    print(f"  Done -> {out}")
    return True

# All files
files = [
    ("bicycle_bell.wav", 13379, 2.3571),
    ("classical_choir_voice_ahhs.wav", 13379, 2.3571),
    ("dance_drums_ride_bell.wav", 13379, 2.3571),
    ("drum_and_percussion_kick.wav", 13379, 2.3571),
    ("ethnic_flavours_atarigane.wav", 13379, 2.3571),
    ("ethnic_flavours_hyoushigi.wav", 13379, 2.3571),
    ("ethnic_flavours_kotsuzumi.wav", 13379, 2.3571),
    ("ethnic_flavours_ohtsuzumi.wav", 13379, 2.3571),
    ("heart_of_asia_gamelan.wav", 13379, 2.3571),
    ("register_noise.wav", 13379, 2.3571),
    ("sc88pro_accordion.wav", 6689, 4.7146),
    ("sc88pro_accordion_duplicate.wav", 6689, 4.7146),
    ("sc88pro_bubbles.wav", 13379, 2.3571),
    ("sc88pro_church_organ3_high.wav", 13379, 2.3571),
    ("sc88pro_church_organ3_low.wav", 13379, 2.3571),
    ("sc88pro_fingered_bass.wav", 53516, 0.5892),
    ("sc88pro_flute.wav", 3344, 9.4306),
    ("sc88pro_french_horn_60.wav", 13379, 2.3571),
    ("sc88pro_french_horn_72.wav", 6689, 4.7146),
    ("sc88pro_fretless_bass.wav", 53516, 0.5892),
    ("sc88pro_glockenspiel.wav", 3344, 9.4306),
    ("sc88pro_harp.wav", 3344, 9.4306),
    ("sc88pro_jingle_bell.wav", 13379, 2.3571),
    ("sc88pro_mute_high_conga.wav", 13379, 2.3571),
    ("sc88pro_nylon_str_guitar.wav", 6689, 4.7146),
    ("sc88pro_open_low_conga.wav", 13379, 2.3571),
    ("sc88pro_orchestra_cymbal_crash.wav", 13379, 2.3571),
    ("sc88pro_orchestra_snare.wav", 13379, 2.3571),
    ("sc88pro_organ2.wav", 6689, 4.7146),
    ("sc88pro_piano1_48.wav", 26758, 1.1785),
    ("sc88pro_piano1_60.wav", 13379, 2.3571),
    ("sc88pro_piano1_72.wav", 6689, 4.7146),
    ("sc88pro_piano1_84.wav", 1672, 18.8612),
    ("sc88pro_pizzicato_strings.wav", 6689, 4.7146),
    ("sc88pro_rnd_kick.wav", 13379, 2.3571),
    ("sc88pro_rnd_snare.wav", 11025, 2.8604),
    ("sc88pro_slap_bass.wav", 44100, 0.7151),
    ("sc88pro_square_wave.wav", 6689, 4.7146),
    ("sc88pro_string_ensemble_60.wav", 13379, 2.3571),
    ("sc88pro_string_ensemble_72.wav", 6689, 4.7146),
    ("sc88pro_string_ensemble_84.wav", 3344, 9.4306),
    ("sc88pro_synth_bass.wav", 22050, 1.4302),
    ("sc88pro_taiko.wav", 13379, 2.3571),
    ("sc88pro_tambourine.wav", 13379, 2.3571),
    ("sc88pro_timpani.wav", 26758, 1.1785),
    ("sc88pro_timpani_with_snare.wav", 26758, 1.1785),
    ("sc88pro_tr909_hand_clap.wav", 13379, 2.3571),
    ("sc88pro_trumpet_60.wav", 13379, 2.3571),
    ("sc88pro_trumpet_72.wav", 6689, 4.7146),
    ("sc88pro_trumpet_84.wav", 3344, 9.4306),
    ("sc88pro_tuba_39.wav", 44100, 0.7151),
    ("sc88pro_tuba_51.wav", 22050, 1.4302),
    ("sc88pro_tubular_bell.wav", 6689, 4.7146),
    ("sc88pro_wind.wav", 13379, 2.3571),
    ("sc88pro_xylophone.wav", 3344, 9.4306),
    ("sd90_ambient_tom.wav", 13379, 2.3571),
    ("sd90_classical_detuned_ep1_high.wav", 13379, 2.3571),
    ("sd90_classical_detuned_ep1_low.wav", 13379, 2.3571),
    ("sd90_classical_distortion_guitar_high.wav", 13379, 2.3571),
    ("sd90_classical_distortion_guitar_low.wav", 13379, 2.3571),
    ("sd90_classical_oboe.wav", 13379, 2.3571),
    ("sd90_classical_overdrive_guitar.wav", 13379, 2.3571),
    ("sd90_classical_shakuhachi.wav", 13379, 2.3571),
    ("sd90_classical_whistle.wav", 13379, 2.3571),
    ("sd90_cowbell.wav", 13379, 2.3571),
    ("sd90_enhanced_delay_shaku.wav", 13379, 2.3571),
    ("sd90_open_triangle.wav", 13379, 2.3571),
    ("sd90_solo_snare.wav", 13379, 2.3571),
    ("sd90_special_scream_drive.wav", 13379, 2.3571),
    ("steinway_b_piano.wav", 13379, 2.3571),
    ("trinity_30303_mega_bass.wav", 6689, 4.7146),
    ("trinity_big_boned.wav", 26758, 1.1785),
    ("trinity_cymbal_crash.wav", 11025, 2.8604),
    ("unknown_01.wav", 13379, 2.3571),
    ("unknown_02.wav", 13379, 2.3571),
    ("unknown_03.wav", 13379, 2.3571),
    ("unknown_04.wav", 13379, 2.3571),
    ("unknown_05.wav", 13379, 2.3571),
    ("unknown_06.wav", 13379, 2.3571),
    ("unknown_07.wav", 13379, 2.3571),
    ("unknown_08.wav", 13379, 2.3571),
    ("unknown_09.wav", 13379, 2.3571),
    ("unknown_10.wav", 13379, 2.3571),
    ("unknown_11.wav", 11025, 2.8604),
    ("unknown_12.wav", 10512, 3.0000),
    ("unknown_13.wav", 10512, 3.0000),
    ("unknown_14.wav", 10512, 3.0000),
    ("unknown_15.wav", 10512, 3.0000),
    ("unknown_16.wav", 10512, 3.0000),
    ("unknown_17.wav", 10512, 3.0000),
    ("unknown_18.wav", 13379, 2.3571),
    ("unknown_bell.wav", 11025, 2.8604),
    ("unknown_close_hihat.wav", 13379, 2.3571),
    ("unknown_female_voice.wav", 13379, 2.3571),
    ("unknown_koto_high.wav", 13379, 2.3571),
    ("unknown_koto_low.wav", 13379, 2.3571),
    ("unknown_open_hihat.wav", 13379, 2.3571),
    ("unknown_snare.wav", 13379, 2.3571),
    ("unknown_synth_snare.wav", 11025, 2.8604),
    ("unused_guitar_separates_power_chord.wav", 13379, 2.3571),
    ("unused_heart_of_asia_indian_drum.wav", 13379, 2.3571),
    ("unused_sc55_tom.wav", 11025, 2.8604),
    ("unused_sc88pro_unison_slap.wav", 13379, 2.3571),
    ("unused_sd90_oboe.wav", 13379, 2.3571),
    ("unused_unknown_male_voice.wav", 13379, 2.3571),
]

for f, r, fac in files:
    convert_file(f, r, fac)

print("\nDone! Files in converted/")