#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "sbagenxlib.h"

#define EXPECT_EQ(field, value_expr) \
  do { \
    size_t expected__ = (size_t)(value_expr); \
    if (info.field != expected__) { \
      fprintf(stderr, "FAIL: %s expected %zu got %zu\n", #field, expected__, (size_t)info.field); \
      exit(1); \
    } \
  } while (0)

int main(void) {
  SbxAbiLayoutInfo info;
  sbx_fill_abi_layout_info(&info);

  EXPECT_EQ(amp_adjust_point_size, sizeof(SbxAmpAdjustPoint));
  EXPECT_EQ(amp_adjust_point_adj_offset, offsetof(SbxAmpAdjustPoint, adj));
  EXPECT_EQ(amp_adjust_spec_size, sizeof(SbxAmpAdjustSpec));
  EXPECT_EQ(amp_adjust_spec_points_offset, offsetof(SbxAmpAdjustSpec, points));
  EXPECT_EQ(mix_mod_spec_size, sizeof(SbxMixModSpec));
  EXPECT_EQ(mix_mod_spec_end_level_offset, offsetof(SbxMixModSpec, end_level));
  EXPECT_EQ(mix_mod_spec_wake_enabled_offset, offsetof(SbxMixModSpec, wake_enabled));
  EXPECT_EQ(iso_envelope_spec_size, sizeof(SbxIsoEnvelopeSpec));
  EXPECT_EQ(iso_envelope_spec_release_offset, offsetof(SbxIsoEnvelopeSpec, release));
  EXPECT_EQ(iso_envelope_spec_edge_mode_offset, offsetof(SbxIsoEnvelopeSpec, edge_mode));
  EXPECT_EQ(mix_fx_spec_size, sizeof(SbxMixFxSpec));
  EXPECT_EQ(mix_fx_spec_envelope_waveform_offset, offsetof(SbxMixFxSpec, envelope_waveform));
  EXPECT_EQ(mix_fx_spec_motion_waveform_offset, offsetof(SbxMixFxSpec, motion_waveform));
  EXPECT_EQ(mix_fx_spec_amp_offset, offsetof(SbxMixFxSpec, amp));
  EXPECT_EQ(mix_fx_spec_mixam_mode_offset, offsetof(SbxMixFxSpec, mixam_mode));
  EXPECT_EQ(mix_fx_spec_mixam_floor_offset, offsetof(SbxMixFxSpec, mixam_floor));
  EXPECT_EQ(mix_fx_spec_mixam_bind_program_beat_offset, offsetof(SbxMixFxSpec, mixam_bind_program_beat));
  EXPECT_EQ(mix_amp_keyframe_size, sizeof(SbxMixAmpKeyframe));
  EXPECT_EQ(mix_amp_keyframe_interp_offset, offsetof(SbxMixAmpKeyframe, interp));
  EXPECT_EQ(engine_config_size, sizeof(SbxEngineConfig));
  EXPECT_EQ(engine_config_channels_offset, offsetof(SbxEngineConfig, channels));
  EXPECT_EQ(pcm_convert_state_size, sizeof(SbxPcmConvertState));
  EXPECT_EQ(pcm_convert_state_dither_mode_offset, offsetof(SbxPcmConvertState, dither_mode));
  EXPECT_EQ(audio_writer_config_size, sizeof(SbxAudioWriterConfig));
  EXPECT_EQ(audio_writer_config_format_offset, offsetof(SbxAudioWriterConfig, format));
  EXPECT_EQ(audio_writer_config_mp3_vbr_quality_offset, offsetof(SbxAudioWriterConfig, mp3_vbr_quality));
  EXPECT_EQ(audio_writer_config_prefer_float_input_offset, offsetof(SbxAudioWriterConfig, prefer_float_input));
  EXPECT_EQ(tone_spec_size, sizeof(SbxToneSpec));
  EXPECT_EQ(tone_spec_amplitude_offset, offsetof(SbxToneSpec, amplitude));
  EXPECT_EQ(tone_spec_waveform_offset, offsetof(SbxToneSpec, waveform));
  EXPECT_EQ(tone_spec_noise_waveform_offset, offsetof(SbxToneSpec, noise_waveform));
  EXPECT_EQ(tone_spec_iso_release_offset, offsetof(SbxToneSpec, iso_release));
  EXPECT_EQ(tone_spec_iso_edge_mode_offset, offsetof(SbxToneSpec, iso_edge_mode));
  EXPECT_EQ(program_keyframe_size, sizeof(SbxProgramKeyframe));
  EXPECT_EQ(program_keyframe_tone_offset, offsetof(SbxProgramKeyframe, tone));
  EXPECT_EQ(program_keyframe_interp_offset, offsetof(SbxProgramKeyframe, interp));
  EXPECT_EQ(mix_input_config_size, sizeof(SbxMixInputConfig));
  EXPECT_EQ(mix_input_config_looper_spec_override_offset, offsetof(SbxMixInputConfig, looper_spec_override));
  EXPECT_EQ(mix_input_config_warn_user_offset, offsetof(SbxMixInputConfig, warn_user));
  EXPECT_EQ(safe_seqfile_preamble_size, sizeof(SbxSafeSeqfilePreamble));
  EXPECT_EQ(safe_seqfile_preamble_amp_adjust_offset, offsetof(SbxSafeSeqfilePreamble, amp_adjust));
  EXPECT_EQ(safe_seqfile_preamble_mix_mod_offset, offsetof(SbxSafeSeqfilePreamble, mix_mod));
  EXPECT_EQ(safe_seqfile_preamble_iso_env_offset, offsetof(SbxSafeSeqfilePreamble, iso_env));
  EXPECT_EQ(safe_seqfile_preamble_mixam_env_offset, offsetof(SbxSafeSeqfilePreamble, mixam_env));
  EXPECT_EQ(safe_seqfile_preamble_have_K_offset, offsetof(SbxSafeSeqfilePreamble, have_K));
  EXPECT_EQ(safe_seqfile_preamble_mix_path_offset, offsetof(SbxSafeSeqfilePreamble, mix_path));
  EXPECT_EQ(safe_seqfile_preamble_out_path_offset, offsetof(SbxSafeSeqfilePreamble, out_path));
  EXPECT_EQ(diagnostic_size, sizeof(SbxDiagnostic));
  EXPECT_EQ(diagnostic_message_offset, offsetof(SbxDiagnostic, message));
  EXPECT_EQ(curve_info_size, sizeof(SbxCurveInfo));
  EXPECT_EQ(curve_info_mixamp_piece_count_offset, offsetof(SbxCurveInfo, mixamp_piece_count));
  EXPECT_EQ(curve_eval_config_size, sizeof(SbxCurveEvalConfig));
  EXPECT_EQ(curve_eval_config_wake_min_offset, offsetof(SbxCurveEvalConfig, wake_min));
  EXPECT_EQ(curve_eval_config_mix_amp0_pct_offset, offsetof(SbxCurveEvalConfig, mix_amp0_pct));
  EXPECT_EQ(curve_source_config_size, sizeof(SbxCurveSourceConfig));
  EXPECT_EQ(curve_source_config_iso_release_offset, offsetof(SbxCurveSourceConfig, iso_release));
  EXPECT_EQ(curve_source_config_duration_sec_offset, offsetof(SbxCurveSourceConfig, duration_sec));
  EXPECT_EQ(curve_source_config_loop_offset, offsetof(SbxCurveSourceConfig, loop));
  EXPECT_EQ(builtin_drop_config_size, sizeof(SbxBuiltinDropConfig));
  EXPECT_EQ(builtin_drop_config_beat_target_hz_offset, offsetof(SbxBuiltinDropConfig, beat_target_hz));
  EXPECT_EQ(builtin_drop_config_wake_sec_offset, offsetof(SbxBuiltinDropConfig, wake_sec));
  EXPECT_EQ(builtin_drop_config_fade_sec_offset, offsetof(SbxBuiltinDropConfig, fade_sec));
  EXPECT_EQ(builtin_sigmoid_config_size, sizeof(SbxBuiltinSigmoidConfig));
  EXPECT_EQ(builtin_sigmoid_config_beat_target_hz_offset, offsetof(SbxBuiltinSigmoidConfig, beat_target_hz));
  EXPECT_EQ(builtin_sigmoid_config_wake_sec_offset, offsetof(SbxBuiltinSigmoidConfig, wake_sec));
  EXPECT_EQ(builtin_sigmoid_config_sig_h_offset, offsetof(SbxBuiltinSigmoidConfig, sig_h));
  EXPECT_EQ(builtin_slide_config_size, sizeof(SbxBuiltinSlideConfig));
  EXPECT_EQ(builtin_slide_config_carrier_end_hz_offset, offsetof(SbxBuiltinSlideConfig, carrier_end_hz));
  EXPECT_EQ(builtin_slide_config_fade_sec_offset, offsetof(SbxBuiltinSlideConfig, fade_sec));
  EXPECT_EQ(curve_timeline_config_size, sizeof(SbxCurveTimelineConfig));
  EXPECT_EQ(curve_timeline_config_wake_sec_offset, offsetof(SbxCurveTimelineConfig, wake_sec));
  EXPECT_EQ(curve_timeline_config_mute_program_tone_offset, offsetof(SbxCurveTimelineConfig, mute_program_tone));
  EXPECT_EQ(curve_timeline_config_fade_sec_offset, offsetof(SbxCurveTimelineConfig, fade_sec));
  EXPECT_EQ(curve_timeline_size, sizeof(SbxCurveTimeline));
  EXPECT_EQ(curve_timeline_mix_frames_offset, offsetof(SbxCurveTimeline, mix_frames));
  EXPECT_EQ(curve_timeline_mix_frame_count_offset, offsetof(SbxCurveTimeline, mix_frame_count));
  EXPECT_EQ(runtime_context_config_size, sizeof(SbxRuntimeContextConfig));
  EXPECT_EQ(runtime_context_config_default_mix_amp_pct_offset, offsetof(SbxRuntimeContextConfig, default_mix_amp_pct));
  EXPECT_EQ(runtime_context_config_aux_tones_offset, offsetof(SbxRuntimeContextConfig, aux_tones));
  EXPECT_EQ(runtime_context_config_amp_adjust_offset, offsetof(SbxRuntimeContextConfig, amp_adjust));

  printf("PASS: sbagenxlib ABI layout info checks\n");
  return 0;
}
