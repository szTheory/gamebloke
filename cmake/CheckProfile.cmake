# Profile validator (D-01, D-02, D-03, D-05, D-06, D-07, D-08, D-10, PROF-01).
#
# Script mode:   cmake -DPROFILE=<profiles/agb.json> -DDOC=<docs/profile-agb.md> \
#                      -DREGISTRY=<registry/disputes.json> -DVOCAB=<contracts/evidence.json> \
#                      -P cmake/CheckProfile.cmake
#
# profiles/agb.json is the authority for the machine profile; docs/profile-agb.md
# is the narrative. This check reads the vocabulary and the dispute registry
# first (their own findings are reported), loads the profile, validates it, then
# compares the profile policy tables, unmodeled list, and version with the document.
#
# Reason codes (printed as `profile: <code>: <detail>`, every finding reported):
#   usage                  script mode without -DPROFILE=, -DDOC=, -DREGISTRY=, or -DVOCAB=
#   bad-schema             schema is not exactly gamebloke.profile/1
#   bad-version            profile_version is not a number whose text is a positive integer
#   missing-field          schema, profile_version, id, machine, boot, bios_provider, cart,
#                          timing, or unmodeled absent, null, or empty; any of the eleven
#                          timing keys absent or not an object; a numeric timing value absent
#   bad-machine            id or machine is not exactly `agb`
#   bad-boot               boot is not `direct-start`
#   bad-bios-provider      bios_provider fails gbl_bios_provider_valid (cmake/CheckContract.cmake)
#   bad-cart               cart is not an object or its kind is not `rom-only`
#   missing-source         a timing entry without `source`, or a non-UNK entry without a
#                          non-empty `cite`
#   bad-source-label       a source outside the vocabulary labels, or a null value under a
#                          non-UNK label
#   timing-arithmetic      one finding per broken invariant: dots_visible + dots_hblank =
#                          dots_per_line; dots_per_line * cycles_per_dot = cycles_per_line;
#                          lines_visible + lines_vblank = lines_per_frame; cycles_per_line *
#                          lines_per_frame = cycles_per_frame; master_clock_hz = 2^24
#   hblank-edge-hardcoded  hblank_flag_edge_cycle.value is anything but null
#   ppu-policy-missing    ppu_policy is absent, null, or not an object
#   ppu-policy-value      a selected PPU policy value, provenance, dispute, or
#                          hardware qualification differs from its contract
#   ppu-policy-doc        the PPU policy table is absent, stale, duplicated, or malformed
#   ppu-object-policy-value the exact object support/work policy has a missing, extra,
#                          mistyped, or altered field
#   ppu-object-policy-doc the canonical object-policy table is stale or malformed
#   ppu-composition-policy-value the exact layer-order/tie policy is wrong
#   ppu-composition-policy-doc the canonical composition-policy table is stale or malformed
#   ppu-capability-value  display capability shape, source layout, dimensions,
#                         implementation status, or raw-output geometry is wrong
#   ppu-capability-doc    the display capability or raw-output narrative is stale
#   ppu-access-policy-value video-memory master/region/state policy or cost binding is wrong
#   ppu-access-policy-doc the video-memory access narrative is stale
#   unmodeled-missing      one finding per required unmodeled id that is absent
#   unmodeled-bad-entry    an entry that is not an object; an id that is not
#                          ^[a-z0-9][a-z0-9-]*$ or is duplicated; a status other than
#                          `unmodeled`; a missing or empty reason; sp-micro-differences
#                          details lacking an item that names 0x04000800 or one that names
#                          the cartridge switch; hblank-flag-edge without its dispute
#   dispute-unknown        a `dispute` value in timing or unmodeled that is not a registry
#                          id, or the hblank_flag_edge_cycle entry with no dispute
#   doc-no-section         the document has no `## Unmodeled` heading or the section has no rows
#   doc-missing-id         a JSON unmodeled id is absent from the document table
#   doc-extra-id           a document table id is absent from the JSON
#   doc-version-mismatch   no `profile_version: N` line in the document, or N differs
#   bus-policy-missing     bus_policy is absent or null
#   bus-policy-value       bus policy structure, request costs, width/class enums, or unit is wrong
#   bus-policy-source      bus policy is not project-authored with its explicit citation
#   bus-policy-doc         bus policy narrative is missing, stale, or has an extra row
#   init-policy            initialization policy structure, provenance, or zero value is wrong
#   read-policy            read policy structure, provenance, or placeholder value is wrong
#   cpu-policy-missing     cpu_policy is absent, null, or not an object
#   cpu-policy-source      CPU policy or sourced ARM architecture evidence is inflated or stale
#   cpu-policy-value       CPU direct-start values or supported/excluded instruction scope is wrong
#   cpu-policy-doc         visible CPU policy or instruction-scope table is missing, stale, or duplicated
#   irq-policy-missing     irq_input_power_policy is absent, null, or not an object
#   irq-policy-source      Phase 8 policy source or cited hardware source row is stale
#   irq-policy-value       Phase 8 policy value, source row set, or profile version is wrong
#   irq-policy-doc         visible IRQ/input/power policy or source-fact table is stale
#   audio-policy-missing   audio_policy is absent or not an object
#   audio-policy-value     exact capture, register, Direct Sound FIFO/DMA,
#                          SOUNDBIAS, or qualification contract differs
#   audio-policy-doc       canonical audio policy/register tables are absent or stale
# The json:, contract:, and registry: codes come from the loader and the vocabulary
# and registry readers this file calls.

# Script mode starts with every policy OLD (IN_LIST is a plain argument under
# CMP0057 OLD on the 3.25 floor); pin the floor policy set.
cmake_minimum_required(VERSION 3.25)
include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/CheckRegistry.cmake")

set(GBL_PROFILE_TIMING_KEYS
  master_clock_hz cycles_per_dot dots_visible dots_hblank dots_per_line
  cycles_per_line lines_visible lines_vblank lines_per_frame cycles_per_frame
  hblank_flag_edge_cycle)
set(GBL_PROFILE_REQUIRED_UNMODELED
  sp-micro-differences prefetch waitcnt bus-costs timer-edge-qualification
  hblank-flag-edge ppu audio dma irq-controller backup-media bios-protection
  io-devices)
set(GBL_PROFILE_AUDIO_POLICY_ROWS
  "model|gamebloke-audio-model-4|STRING"
  "classification|project-selected|STRING"
  "hardware_qualification|UNKNOWN|STRING"
  "tap|pre-pwm|STRING"
  "encoding|signed-s16le|STRING"
  "channel_layout|stereo-interleaved-left-first|STRING"
  "sample_rate_hz|32768|NUMBER"
  "bytes_per_frame|4|NUMBER"
  "master_cycles_per_frame|512|NUMBER"
  "reset_epoch_cycle|0|NUMBER"
  "first_sample_cycle|512|NUMBER"
  "interval|start-exclusive-end-inclusive|STRING"
  "sample_value|six-source-mixed-signed-s16le|STRING"
  "psg_channel_count|4|NUMBER"
  "direct_sound_channel_count|2|NUMBER"
  "psg_source_centering|per-channel-(2*unsigned-level-15)*2048|STRING"
  "psg_route_gain_policy|SOUNDCNT_L-independent-left-right-routes,side-level-0-through-7-over-7-applies-only-to-PSG|STRING"
  "psg_group_ratio_policy|SOUNDCNT_H-code-0-quarter,code-1-half,code-2-full,code-3-mutes-PSG|STRING"
  "direct_sound_route_gain_policy|FIFO-A/B-independent-left-right-routes,signed-S8-times-256,half-or-full-gain,SOUNDCNT_L-side-level-excluded|STRING"
  "master_mute_policy|SOUNDCNT_X-master-off-silences-both-sides|STRING"
  "mix_accumulator_policy|left-and-right-independent-signed-64-bit-numerators,common-denominator-28|STRING"
  "mix_rounding_policy|one-final-nearest-ties-away-from-zero-after-summing|STRING"
  "mix_clipping_policy|round-then-saturate-to-signed-16-bit|STRING"
  "modeled_sources|psg-channel-sources-1-2-3-4,direct-sound-fifo-a-b-signed-samples|STRING"
  "direct_sound_fifo_ports|A=0x040000A0,B=0x040000A4|STRING"
  "direct_sound_fifo_capacity_bytes|32|NUMBER"
  "direct_sound_sample_encoding|signed-s8|STRING"
  "direct_sound_write_policy|widths-1-2-4,increasing-address-lane-order,narrow-write-merges-into-next-ring-word-retaining-untouched-lanes-and-consuming-one-word|STRING"
  "direct_sound_overflow_policy|flush-queue-and-cursors-then-accept-whole-current-write|STRING"
  "direct_sound_underflow_policy|hold-last-signed-sample,reset-latch-zero|STRING"
  "direct_sound_fifo_reset_policy|selected-queue-cursors-latch-and-pending-request-only,timers-unaffected|STRING"
  "direct_sound_master_disable_policy|both-queues-cursors-latches-and-pending-requests,timers-continue|STRING"
  "direct_sound_timer_policy|actual-selected-timer-overflow-only,timer1-cascade-pops-only-on-real-wrap,each-matching-fifo-pops-once|STRING"
  "direct_sound_same_cycle_order|timer-pop-and-readiness,PSG-progression,sample-old-route,completing-bus-write|STRING"
  "direct_sound_fifo_request_threshold_bytes|16|NUMBER"
  "direct_sound_fifo_request_policy|post-pop-exactly-16,no-duplicate-pending-or-active-request|STRING"
  "direct_sound_dma_channel_map|fifo-a-dma1,fifo-b-dma2|STRING"
  "direct_sound_dma_transfer_policy|four-32-bit-words,fixed-mapped-fifo-destination,ignore-programmed-count-width-and-destination-mode|STRING"
  "direct_sound_dma_source_policy|align-programmed-source-to-word,increment-decrement-or-fixed-in-4-byte-units,validate-all-four-lanes-in-one-modeled-region|STRING"
  "direct_sound_dma_supported_setup|enabled-special-audio-timing-only|STRING"
  "direct_sound_dma_unsupported_policy|consume-sample-ignore-refill,no-programmed-destination-fallback|STRING"
  "direct_sound_dma_rearm_policy|later-threshold-independent-of-repeat-bit,preserve-source-progression|STRING"
  "direct_sound_dma_irq_policy|set-matching-channel-if-on-completion-only-when-control-irq-enabled|STRING"
  "direct_sound_dma_priority_policy|reselect-after-startup-and-each-sound-word,higher-priority-runs-at-next-boundary-before-resume|STRING"
  "direct_sound_dma_publication_policy|whole-private-machine-candidate,all-interleaved-effects-and-sound-words-commit-or-refuse|STRING"
  "direct_sound_primary_source|AGB Programming Manual v1.1 §§10.2, 10.3, 10.7 and 12.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf)|STRING"
  "direct_sound_supplemental_source|GBATEK Sound and DMA (community reverse-engineered supplement, https://mgba-emu.github.io/gbatek/)|STRING"
  "direct_sound_secondary_report|mGBA issue #1847, maintainer report with hedged empty and overflow observations (https://github.com/mgba-emu/mgba/issues/1847)|STRING"
  "direct_sound_policy_classification|project-selected|STRING"
  "direct_sound_hardware_qualification|UNKNOWN|STRING"
  "capture_bound_scope|caller-owned-session|STRING"
  "capture_session_start|explicit-current-machine-cycle|STRING"
  "capture_session_progress|accepted-contiguous-operations|STRING"
  "capture_session_global_grid|unchanged|STRING"
  "max_capture_cycles|16777216|NUMBER"
  "max_capture_frames|32768|NUMBER"
  "max_capture_bytes|131072|NUMBER"
  "refusal_postcondition|machine-output-session-progress-unchanged|STRING"
  "event_dispatch_limit_per_operation|4096|NUMBER"
  "same_cycle_order|ordered-device-effects-including-timer-overflow-and-dma-readiness,then-PSG-progression,then-sample-old-route,then-completing-bus-write-including-dma-data-write|STRING"
  "psg_primary_source|AGB Programming Manual v1.1 §§10.3-10.6 consulted via third-party-hosted copy|STRING"
  "psg_supplemental_source|GBATEK 3.06 community reverse-engineered supplement|STRING"
  "psg_sequencer_period_cycles|32768|NUMBER"
  "psg_sequencer_rate_hz|512|NUMBER"
  "psg_sequencer_epoch|reset-cycle-zero|STRING"
  "psg_sequencer_first_tick_cycle|32768|NUMBER"
  "psg_sequencer_first_step|0|NUMBER"
  "psg_sequencer_steps|length-0-2-4-6,sweep-channel-1-steps-2-and-6,envelope-7|STRING"
  "psg_sequencer_master_off_phase|reset-anchored-cycle-phase-continues|STRING"
  "psg_trigger_policy|completion-anchored-trigger,resets-source-phase,square-phase-zero-high,requires-channel-DAC|STRING"
  "psg_ch1_frequency_policy|effective-frequency-and-sweep-controls-latch-on-trigger,guest-readback-remains-programmed|STRING"
  "psg_reset_policy|machine-reset-clears-channel-state-status-and-both-Wave-RAM-banks|STRING"
  "psg_master_off_policy|clears-channel-registers-and-source-state,retains-Wave-RAM,keeps-sequencer-reset-anchor|STRING"
  "psg_dac_disable_policy|clears-that-channel-source-state-and-status,CH3-Wave-RAM-aperture-remains-accessible|STRING"
  "psg_channel_status_policy|separate-from-trigger-and-length-enable,trigger-sets-when-DAC-enabled,length-expiry-or-DAC-disable-clears|STRING"
  "psg_length_policy|trigger-loads-maximum-minus-register,next-eligible-clock-decrements,zero-register-loads-full-maximum,enabled-only|STRING"
  "psg_length_maxima|ch1=64,ch2=64,ch3=256,ch4=64|STRING"
  "psg_envelope_policy|channels-1-2-4,trigger-latches-volume-direction-period,step-7,period-zero-disabled,saturates-at-0-or-15|STRING"
  "psg_square_period_cycles|128*(2048-fdat)|STRING"
  "psg_square_duty_policy|duty-high-ratios-1-2-4-6-eighths|STRING"
  "psg_ch1_sweep_policy|trigger-latched-controls-and-effective-frequency,step-2-and-6,shift-zero-no-change,period-zero-disabled,subtract-delta-bounded-by-current-frequency,zero-stays-zero,overflow-above-2047-clears-active-status|STRING"
  "psg_noise_divider_formula|32*r*2^(shift+1),ratio-zero-r=1/2|STRING"
  "psg_noise_width_bits|15-or-7|STRING"
  "psg_noise_seeds|15-bit=0x4000,7-bit=0x40|STRING"
  "psg_noise_feedback_masks|15-bit=0x6000,7-bit=0x60|STRING"
  "psg_noise_lfsr_step|right-shift-xor-feedback-using-width-specific-mask|STRING"
  "psg_noise_prohibited_shifts|14,15|STRING"
  "psg_noise_prohibited_behavior|holds-LFSR-phase-and-noise-bit,length-and-envelope-clocks-continue|STRING"
  "psg_wave_bank_count|2|NUMBER"
  "psg_wave_bank_bytes|16|NUMBER"
  "psg_wave_samples_per_bank|32|NUMBER"
  "psg_wave_total_bytes|32|NUMBER"
  "psg_wave_guest_aperture|0x04000090-0x0400009F|STRING"
  "psg_wave_guest_widths|1,2,4|STRING"
  "psg_wave_guest_lane_order|little-endian|STRING"
  "psg_wave_guest_bank_policy|bank-opposite-SOUND3CNT_L-front-bank|STRING"
  "psg_wave_pattern_length_nibbles|32-or-64|STRING"
  "psg_wave_64_nibble_order|selected-front-bank-then-other-bank|STRING"
  "psg_wave_nibble_order|high-then-low|STRING"
  "psg_wave_nibble_period_cycles|4*(2048-fdat)|STRING"
  "psg_wave_volume_policy|field-0-mute-1-full-2-half-3-quarter,force-bit-three-quarter|STRING"
  "psg_live_update_policy|CH1-frequency-and-sweep-controls-trigger-latched,CH2-frequency-keeps-square-phase-modulo-new-period,CH3-frequency-normalizes-nibble-cycle-remainder-and-pattern-write-normalizes-index,CH4-width-change-reseeds,supported-divider-frequency-writes-normalize-noise-cycle-remainder,same-width-prohibited-divider-writes-preserve-noise-phase-and-remainder|STRING"
  "psg_ch3_rate_policy|manual-whole-pattern-derived-4*(2048-fdat)-cycles-per-nibble|STRING"
  "psg_ch3_rate_conflict|GBATEK-nibble-rate-interpretation-differs-by-factor-of-two|STRING"
  "psg_ch3_length_source_note|AGB-manual-CH3-expiration-paragraph-names-CH2-status-bit,model-uses-CH3-status|STRING"
  "psg_wave_retrigger_corruption|unmodeled|STRING")
set(GBL_PROFILE_AUDIO_SOUNDBIAS_JSON [=[{"address":"0x04000088-0x04000089","writable_mask":"0xC3FF","reset_value":"0x0200","master_off_access":"available","cadence_effect":"none","capture_quantization":"none"}]=])
set(GBL_PROFILE_AUDIO_SOUNDBIAS_FIELDS
  address writable_mask reset_value master_off_access cadence_effect
  capture_quantization)
set(GBL_PROFILE_AUDIO_REGISTERS_JSON [=[[
  {"name":"SOUND1CNT_L","address":"0x04000060","retained_mask":"0x007F","read_mask":"0x007F","action_mask":"0x0000","effect":"sweep-control"},
  {"name":"SOUND1CNT_H","address":"0x04000062","retained_mask":"0xFFFF","read_mask":"0xFFC0","action_mask":"0x0000","effect":"length-envelope-duty-control"},
  {"name":"SOUND1CNT_X","address":"0x04000064","retained_mask":"0x47FF","read_mask":"0x47FF","action_mask":"0x8000","effect":"length-enable-retained-trigger-self-clears-separate-status"},
  {"name":"SOUND2CNT_L","address":"0x04000068","retained_mask":"0xFFFF","read_mask":"0xFFC0","action_mask":"0x0000","effect":"length-envelope-duty-control"},
  {"name":"SOUND2CNT_H","address":"0x0400006C","retained_mask":"0x47FF","read_mask":"0x47FF","action_mask":"0x8000","effect":"length-enable-retained-trigger-self-clears-separate-status"},
  {"name":"SOUND3CNT_L","address":"0x04000070","retained_mask":"0x00E0","read_mask":"0x00E0","action_mask":"0x0000","effect":"DAC-bank-and-dimension-control"},
  {"name":"SOUND3CNT_H","address":"0x04000072","retained_mask":"0xE0FF","read_mask":"0xE000","action_mask":"0x0000","effect":"length-data-and-volume-control"},
  {"name":"SOUND3CNT_X","address":"0x04000074","retained_mask":"0x47FF","read_mask":"0x47FF","action_mask":"0x8000","effect":"length-enable-retained-trigger-self-clears-separate-status"},
  {"name":"SOUND4CNT_L","address":"0x04000078","retained_mask":"0xFF3F","read_mask":"0xFF00","action_mask":"0x0000","effect":"length-and-envelope-control"},
  {"name":"SOUND4CNT_H","address":"0x0400007C","retained_mask":"0x40FF","read_mask":"0x40FF","action_mask":"0x8000","effect":"length-enable-retained-trigger-self-clears-separate-status"},
  {"name":"SOUNDCNT_L","address":"0x04000080","retained_mask":"0xFF77","read_mask":"0xFF77","action_mask":"0x0000","effect":"psg-routing-and-volume-state"},
  {"name":"SOUNDCNT_H","address":"0x04000082","retained_mask":"0x770F","read_mask":"0x770F","action_mask":"0x8800","effect":"direct-sound-routing-and-volume-state-fifo-reset-actions"},
  {"name":"SOUNDCNT_X","address":"0x04000084","retained_mask":"0x0080","read_mask":"0x008F","action_mask":"0x0000","effect":"master-enable-dynamic-channel-status-disable-clears-channel-state"}
]]=])
set(GBL_PROFILE_HBLANK_DISPUTE disp-hblank-flag-edge)
set(GBL_PROFILE_PPU_POLICY_ROWS
  "policy_classification|project-selected|STRING"
  "hardware_qualification|UNKNOWN|STRING"
  "hardware_source_reference|Nintendo AGB Programming Manual v1.1 §§4.1.1–4.1.2, 5.1.1, 6.1.3–6.1.5, 6.1.7, 6.3.1–6.3.4 and 6.4, printed pp. 58–72 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf)|STRING"
  "geometry|240x160 visible pixels, 308 dots per line, 228 lines per frame|STRING"
  "line_cycles|1232|NUMBER"
  "nominal_visible_dot_cycles|960|NUMBER"
  "recurrence|one recurring priority-9 event alternates line-start and HBlank boundaries|STRING"
  "event_priority|9|NUMBER"
  "line_transitions|VCOUNT advances 0-227 modulo 228, visible lines are 0-159, VBlank sets at 160 and clears at 0|STRING"
  "same_cycle_order|due machine events precede timed bus effects, row sample precedes a bus write completing on the same cycle|STRING"
  "dispcnt_writable_mask|0xFFFF|STRING"
  "dispstat_writable_mask|0xFF38|STRING"
  "vcount_write_policy|ignored|STRING"
  "compare_write_policy|recompute at write completion after due events|STRING"
  "hblank_event_offset_cycles|1006|NUMBER"
  "hblank_source|REF|STRING"
  "hblank_source_reference|GBATEK, LCD I/O Interrupts and Status, states the flag is 0 for 1,006 cycles, source text is not a project hardware measurement|STRING"
  "hblank_provenance|project-selected|STRING"
  "hblank_dependent_classification|disputed|STRING"
  "hblank_dispute|disp-hblank-flag-edge|STRING"
  "hblank_hardware_qualification|UNKNOWN|STRING"
  "row_sample_point|visible-line-start|STRING"
  "row_same_cycle_write_policy|PPU sample precedes a bus write completing on the sample cycle|STRING"
  "pixel_format|BGR555 little-endian|STRING"
  "frame_validity|complete coverage of 160 supported rows in one frame epoch, published at line 160|STRING"
  "supported_controls|mode 3/4/5 BG2 bitmap sources (mode 3 direct-color 240x160 single page, mode 4 indexed 240x160 two DISPCNT-selected pages, mode 5 direct-color 160x128 two DISPCNT-selected pages), captured BG2 affine transform/reference progression, out-of-extent samples to backdrop, mode-4 index zero transparent and direct-color zero opaque black, non-BG2 background enable bits ignored in bitmap modes, any enabled supported backgrounds per sampled row in modes 0-2 (mode 0 text BG0-BG3, mode 1 text BG0/BG1 or affine BG2, mode 2 affine BG2/BG3), text scroll, four map dimensions, affine transforms and reference progression, 4bpp/8bpp tiles, flips, palette and transparent-index backdrop, regular and affine OBJ rendering, shared BG/OBJ/backdrop priority composition, rectangular WIN0/WIN1 half-open regions, admitted OBJ-window masks and region-specific layer/effect masks, top-and-immediate-next-visible alpha and semitransparent OBJ effects, clamped 5-bit brightness, independent BG/OBJ screen-origin mosaic resampled from the current row, and forced-blank white rows with normal frame coverage|STRING"
  "unmodeled_controls|prohibited OBJ mode/shape encodings and unsupported background-control encodings|STRING"
  "interrupt_if_bits|VBlank 0, HBlank 1, VCOUNT-match 2|STRING"
  "blank_dma_eligibility|HBlank on visible lines 0-159, VBlank at the line 160 transition|STRING")
set(GBL_PROFILE_PPU_OBJECT_POLICY_JSON [=[{"regular_shapes":"square:8x8,16x16,32x32,64x64;horizontal:16x8,32x8,32x16,64x32;vertical:8x16,8x32,16x32,32x64","character_windows":"modes0-2:[0x10000,0x18000);modes3-5:[0x14000,0x18000);full-tile-span-before-read","index_zero_transparency":true,"affine_quantization":"signed-8.8;wide-products;floor-by-256","affine_field":"centered;double-size-destination-only;out-of-source-transparent","coordinate_wrap":"x-modulo-512;y-modulo-256;clip-240x160","unsupported_modes":"prohibited","work_budget_units":1226,"regular_cost":"source-width","affine_cost":"10+2*destination-field-width","eligibility":"enabled-supported-object;vertical-field-intersects-row;no-horizontal-or-transparency-refund","admission_order":"ascending-oam;full-cost-fit;stop-at-first-nonfit","overflow_policy":"valid-partial-scene;omit-all-later-eligible","observation_scope":"internal-row-capture;selected-mask;selected-count;omitted-count;used-work;no-stable-c-api"}]=])
set(GBL_PROFILE_PPU_OBJECT_POLICY_FIELDS
  regular_shapes character_windows index_zero_transparency affine_quantization
  affine_field coordinate_wrap unsupported_modes work_budget_units regular_cost
  affine_cost eligibility admission_order overflow_policy observation_scope)
set(GBL_PROFILE_PPU_COMPOSITION_POLICY_JSON [=[{"priority_order":"ascending-numeric-0-3","bg_tie":"lower-bg-number","obj_tie":"lower-oam-index","obj_bg_tie":"obj-in-front-project-policy","backdrop":"pram-color-zero;behind-all-enabled-opaque-candidates"}]=])
set(GBL_PROFILE_PPU_COMPOSITION_POLICY_FIELDS
  priority_order bg_tie obj_tie obj_bg_tie backdrop)
set(GBL_PROFILE_PPU_WINDOW_POLICY_JSON [=[{"region_order":"win0;win1;obj-window;outside","rectangle_bounds":"half-open;equal-or-reversed-empty;no-wrap","layer_stage":"filter-before-priority;dispcnt-master-gates","disabled_masters":"bypass-winin-winout","obj_coverage":"admitted-nonzero-source-index;no-color;shared-obj-budget","mask_bits":"bg0-bg3;obj;effects;reserved-ignored"}]=])
set(GBL_PROFILE_PPU_WINDOW_POLICY_FIELDS
  region_order rectangle_bounds layer_stage disabled_masters obj_coverage mask_bits)
set(GBL_PROFILE_PPU_EFFECT_POLICY_JSON [=[{"pair":"top-and-immediate-next-visible;no-deeper-search","alpha":"mode1;first-target;second-target;region-effects-enabled","semitransparent_obj":"alpha-with-eligible-next-and-region;ignore-ordinary-first-target-and-mode;no-brightness","coefficient_max":16,"coefficient_input":"low-five-bits;clamp-to-16","channel_arithmetic":"wide-integer;divide16-truncate;clamp0-31","brighten":"c+floor((31-c)*evy/16)","darken":"c-floor(c*evy/16)"}]=])
set(GBL_PROFILE_PPU_EFFECT_POLICY_FIELDS
  pair alpha semitransparent_obj coefficient_max coefficient_input
  channel_arithmetic brighten darken)
set(GBL_PROFILE_PPU_MOSAIC_POLICY_JSON [=[{"dimensions":"field-plus-one;independent-bg-obj","anchors":"visible-screen-origin;x-x%w;y-y%h","sampling":"current-row-captured-registers-and-memory;no-old-row-cache","affine_origin":"current-origin-minus-pb-pd-times-row-anchor-delta","stage":"source-before-destination-window-and-effects","obj_field":"existing-destination-field;anchor-outside-transparent;no-field-growth"}]=])
set(GBL_PROFILE_PPU_MOSAIC_POLICY_FIELDS
  dimensions anchors sampling affine_origin stage obj_field)
set(GBL_PROFILE_PPU_FORCED_BLANK_POLICY_JSON [=[{"color_bgr555":32767,"raw_bytes":"FF 7F","transitions":"next-captured-visible-row;no-extra-delay-or-restart","frame_validity":"normal-epoch-row-coverage;publish-line160","precedence":"bypass-scene-decode;retain-structural-transaction-validation","access_policy":"unchanged-existing-bus-behavior"}]=])
set(GBL_PROFILE_PPU_FORCED_BLANK_POLICY_FIELDS
  color_bgr555 raw_bytes transitions frame_validity precedence access_policy)
set(GBL_PROFILE_PPU_COMPOSITION_FEATURES_JSON [=[{"rectangular_windows":true,"object_windows":true,"alpha_blending":true,"brightness":true,"bg_mosaic":true,"obj_mosaic":true,"forced_blank":true,"classification":"project-selected","hardware_qualification":"UNKNOWN"}]=])
set(GBL_PROFILE_PPU_COMPOSITION_FEATURES_FIELDS
  rectangular_windows object_windows alpha_blending brightness bg_mosaic
  obj_mosaic forced_blank classification hardware_qualification)
set(GBL_PROFILE_PPU_RAW_OUTPUT_JSON [=[{"format":"BGR555_LE","width":240,"height":160,"stride_bytes":480,"byte_length":76800}]=])
set(GBL_PROFILE_PPU_EXPECTED_MODES_JSON [=[
[
  {"mode":0,"implementation_status":"partial","backgrounds":[
    {"bg":0,"source":"text","dimensions":[{"width":256,"height":256},{"width":512,"height":256},{"width":256,"height":512},{"width":512,"height":512}]},
    {"bg":1,"source":"text","dimensions":[{"width":256,"height":256},{"width":512,"height":256},{"width":256,"height":512},{"width":512,"height":512}]},
    {"bg":2,"source":"text","dimensions":[{"width":256,"height":256},{"width":512,"height":256},{"width":256,"height":512},{"width":512,"height":512}]},
    {"bg":3,"source":"text","dimensions":[{"width":256,"height":256},{"width":512,"height":256},{"width":256,"height":512},{"width":512,"height":512}]}
  ]},
  {"mode":1,"implementation_status":"partial","backgrounds":[
    {"bg":0,"source":"text","dimensions":[{"width":256,"height":256},{"width":512,"height":256},{"width":256,"height":512},{"width":512,"height":512}]},
    {"bg":1,"source":"text","dimensions":[{"width":256,"height":256},{"width":512,"height":256},{"width":256,"height":512},{"width":512,"height":512}]},
    {"bg":2,"source":"affine","dimensions":[{"width":128,"height":128},{"width":256,"height":256},{"width":512,"height":512},{"width":1024,"height":1024}]}
  ]},
  {"mode":2,"implementation_status":"partial","backgrounds":[
    {"bg":2,"source":"affine","dimensions":[{"width":128,"height":128},{"width":256,"height":256},{"width":512,"height":512},{"width":1024,"height":1024}]},
    {"bg":3,"source":"affine","dimensions":[{"width":128,"height":128},{"width":256,"height":256},{"width":512,"height":512},{"width":1024,"height":1024}]}
  ]},
  {"mode":3,"implementation_status":"partial","backgrounds":[
    {"bg":2,"source":"direct-color-bitmap","dimensions":[{"width":240,"height":160}]}
  ]},
  {"mode":4,"implementation_status":"partial","backgrounds":[
    {"bg":2,"source":"indexed-bitmap","dimensions":[{"width":240,"height":160}]}
  ]},
  {"mode":5,"implementation_status":"partial","backgrounds":[
    {"bg":2,"source":"direct-color-bitmap","dimensions":[{"width":160,"height":128}]}
  ]}
]
]=])
set(GBL_PROFILE_PPU_VIDEO_MEMORY_ACCESS_JSON [=[{"classification":"project-selected","hardware_qualification":"UNKNOWN","masters":["cpu","dma"],"regions":["pram","vram","oam"],"states":["drawing","hblank","vblank","forced-blank"],"availability":"existing-bus-behavior","ppu_extra_cycles":0,"cost_reference":"bus_policy.request_cycles"}]=])
set(GBL_PROFILE_INT64_MAX 9223372036854775807)
set(GBL_PROFILE_BUS_REQUEST_KEYS bios ewram iwram io pram vram oam rom unmapped)
set(GBL_PROFILE_BUS_REGIONS bios ewram iwram io pram vram oam rom unmapped)
set(GBL_PROFILE_BUS_DIRECTIONS read write)
set(GBL_PROFILE_BUS_WIDTHS 1 2 4)
set(GBL_PROFILE_BUS_SEQUENCE_CLASSES nonsequential sequential)
set(GBL_PROFILE_BUS_ROLES fetch data)
set(GBL_PROFILE_WAITCNT_WINDOWS ws0 ws1 ws2)
set(GBL_PROFILE_TIMING_MODEL_ID gamebloke-timer-model-1)
set(GBL_PROFILE_TIMING_EXECUTION_CITE
  "Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified.")
set(GBL_PROFILE_TIMING_ROM_FORMULA
  "sum over 16-bit beats of (selected WAITCNT N/S wait states for the beat address plus one base cycle)")
set(GBL_PROFILE_TIMING_ROM_EXECUTION_CITE
  "Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf); WAITCNT window and effective beat context select the runtime value; hardware qualification remains UNKNOWN.")
set(GBL_PROFILE_TIMING_NINTENDO_CITE
  "Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf)")
set(GBL_PROFILE_WAITCNT_REG_CITE
  "Gamebloke WAITCNT register implementation in src/machine.c and src/bus.c; the retained mask and reset initialization are project policy.")
set(GBL_PROFILE_WAITCNT_BOUNDARY_PROVENANCE
  "GBATEK 3.06, Game Pak 128 KiB sequential boundary; secondary/community source only.")
set(GBL_PROFILE_DMA_CITE
  "Nintendo AGB Programming Manual v1.1 Chapter 12 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf)")
set(GBL_PROFILE_DMA_TRIGGER_CITE
  "Gamebloke DMA model in src/machine.c; Direct Sound FIFO and the existing specialized VBlank/HBlank PPU routes are implemented; DRQ and other trigger producers remain deferred.")
set(GBL_PROFILE_DMA_TRIGGER_DEFERRED
  "Direct Sound FIFO A/B uses DMA1/DMA2 special timing; VBlank/HBlank retain their specialized PPU routes; DRQ and other trigger producers remain deferred")
set(GBL_PROFILE_DMA_STARTUP_CITE
  "Gamebloke project mapping in include/gamebloke/gamebloke.h; manual activation wording does not qualify a master-cycle conversion.")
set(GBL_PROFILE_DMA_ARBITRATION_CITE
  "Gamebloke selected DMA policy in src/machine.c; project tests establish implementation behavior only.")
set(GBL_PROFILE_DMA_LATCH_CITE
  "registry/disputes.json entry disp-dma-latch; competing observations remain unresolved.")
set(GBL_PROFILE_TIMING_ROM_HARDWARE_CITE
  "Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled.")
set(GBL_PROFILE_TIMING_OTHER_HARDWARE_CITE
  "Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role.")
set(GBL_PROFILE_TIMING_ARM_CITE
  "Arm ARM7TDMI Technical Reference Manual DDI 0210C §§3.3 and 6.20 Table 6-23 (https://documentation-service.arm.com/static/5f4786a179ff4c392c0ff819)")
set(GBL_PROFILE_TIMING_EDGE_CITE
  "Gamebloke timer-model policy in src/machine.c and include/gamebloke/gamebloke.h; not a hardware source.")
set(GBL_PROFILE_BUS_CITE
  "Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled.")
set(GBL_PROFILE_INIT_CITE
  "Gamebloke deterministic project policy: zero initialize state; hardware power-on RAM values remain unknown.")
set(GBL_PROFILE_READ_CITE
  "Unverified Gamebloke project behavior: current-latch placeholders for absent BIOS, unsupported I/O, and ROM tails.")
set(GBL_PROFILE_CPU_CITE
  "Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops.")
set(GBL_PROFILE_CPU_ARCH_CITE
  "ARM7TDMI Technical Reference Manual (DDI 0029G, Issue G), ARM state, Thumb state, and processor modes sections.")
set(GBL_PROFILE_IRQ_POLICY_CITE
  "Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification.")
set(GBL_PROFILE_IRQ_ARM_CITE
  "Arm ARM7TDMI Technical Reference Manual DDI 0210C §§2.8.1–2.8.5 and 6.12 (https://documentation-service.arm.com/static/5f4786a179ff4c392c0ff819)")
set(GBL_PROFILE_IRQ_GBA_CITE
  "Nintendo AGB Programming Manual v1.1 §§14.1–14.2.1, 15, 16.1–16.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf)")
set(GBL_PROFILE_CPU_SUPPORTED
  "ARM data processing: AND, EOR, SUB, RSB, ADD, ADC, SBC, RSC, TST, TEQ, CMP, CMN, ORR, MOV, BIC, MVN with rotated immediate and immediate/register LSL/LSR/ASR/ROR, defined flags and legal PC/exception-return forms"
  "ARM control flow: B, BL, BX"
  "ARM multiply: MUL, MLA, UMULL, UMLAL, SMULL, SMLAL with S variants and captured operands"
  "ARM status transfers: MRS CPSR/SPSR and MSR register/rotated-immediate c/x/s/f masks, privilege and bank rules"
  "ARM single transfer: LDR/STR byte/word, immediate/scaled-register offsets, P/U/B/W/L and post-index T forms, legal PC operands and writeback"
  "ARM subword transfer: LDRH, STRH, LDRSB, LDRSH with immediate/register offsets, pre/post indexing and writeback"
  "ARM block transfer: LDM/STM IA/IB/DA/DB, writeback, user-bank S forms and SPSR-restoring LDM with PC"
  "ARM swap: SWP and SWPB with ordered read/write and captured operands"
  "ARM exception decode: full SWI imm24, reserved UND and absent CDP/MRC/MCR/LDC/STC coprocessor entry before BIOS-none stop"
  "Thumb formats 1-5: immediate shifts, add/subtract, immediate operations, all 16 register ALU operations and legal high-register ADD/CMP/MOV/BX"
  "Thumb formats 6-12: 6 PC-relative LDR, 7 register-offset transfers, 8 signed byte/halfword transfers, 9 immediate-offset byte/word transfers, 10 halfword transfers, 11 SP-relative transfers, 12 PC/SP address formation"
  "Thumb formats 13-19: 13 SP adjustment, 14 PUSH/POP with LR/PC, 15 STMIA/LDMIA, 16 conditional B, 17 SWI imm8, 18 unconditional B, 19 BL prefix/suffix"
  "Executing project policies: short MUL Rd=Rm and SWP/SWPB Rn=Rd or Rn=Rm snapshot aliases, ARM empty/base-in-list reference rules and isolated BL halves/modulo offset arithmetic")
set(GBL_PROFILE_CPU_EXCLUDED
  "Unpredictable operands stop before effects except the explicitly listed executing project policies, including named stops for NV, invalid PC/PSR operands, Thumb same-register MUL, empty Thumb lists and STMIA base-middle/last"
  "Allocated later encodings recognized by the decoder stop as later-architecture-instruction, with no ARMv5+ execution semantics"
  "Unallocated exact-core encodings enter UND, while malformed allocated decode bits are unpredictable"
  "No attached coprocessor or BIOS handler executes, exception entry precedes named BIOS-none stop and vector bytes are never fetched"
  "Static B/BX and ALU-PC target alignment/refill extents retain checked unsupported refusal, not a missing legal instruction family"
  "Original full diagnostic corpus executed locally with admitted fixtures, hosted qualification requires separate exact-head evidence, no hardware, game, BIOS or release qualification is claimed")

# Validate a policy's source and citation as project-authored evidence. Hardware
# claims are not inferred from these placeholders.
function(gbl__profile_policy_source doc policy expected_cite reason)
  gbl_json_string(src "${doc}" ${policy} source)
  gbl_json_string(cite "${doc}" ${policy} cite)
  if(NOT "${src}" STREQUAL "IMPL" OR NOT "${cite}" STREQUAL "${expected_cite}")
    gbl_finding(profile "${reason}" "${policy}: source/citation")
  endif()
endfunction()

# Compare canonical nonnegative decimal strings without converting them to
# integers; CMake's numeric conditions can overflow on out-of-range operands.
function(gbl__profile_decimal_greater out left right)
  string(LENGTH "${left}" left_length)
  string(LENGTH "${right}" right_length)
  if(left_length GREATER right_length)
    set(greater TRUE)
  elseif(left_length LESS right_length)
    set(greater FALSE)
  elseif("${left}" STRGREATER "${right}")
    set(greater TRUE)
  else()
    set(greater FALSE)
  endif()
  set(${out} "${greater}" PARENT_SCOPE)
endfunction()

# Compare canonical nonnegative decimal text with CMake's signed math(EXPR)
# limit without first converting an out-of-range value to an integer.
function(gbl__profile_int64_in_range out value)
  string(LENGTH "${value}" value_length)
  string(LENGTH "${GBL_PROFILE_INT64_MAX}" max_length)
  if(value_length LESS max_length)
    set(in_range TRUE)
  elseif(value_length GREATER max_length)
    set(in_range FALSE)
  else()
    gbl__profile_decimal_greater(exceeds_max "${value}" "${GBL_PROFILE_INT64_MAX}")
    if(exceeds_max)
      set(in_range FALSE)
    else()
      set(in_range TRUE)
    endif()
  endif()
  set(${out} "${in_range}" PARENT_SCOPE)
endfunction()

# Preflight arithmetic using only bounded operands. The subtraction and
# division used for the limits cannot overflow; the requested operation is
# evaluated only after it is proven to fit in signed 64-bit arithmetic.
function(gbl__profile_safe_sum out_ok out_sum left right)
  math(EXPR remaining "${GBL_PROFILE_INT64_MAX}-${right}")
  gbl__profile_decimal_greater(exceeds_limit "${left}" "${remaining}")
  if(exceeds_limit)
    set(${out_ok} FALSE PARENT_SCOPE)
    set(${out_sum} "" PARENT_SCOPE)
  else()
    math(EXPR result "${left}+${right}")
    set(${out_ok} TRUE PARENT_SCOPE)
    set(${out_sum} "${result}" PARENT_SCOPE)
  endif()
endfunction()

function(gbl__profile_safe_product out_ok out_product left right)
  if("${left}" STREQUAL "0" OR "${right}" STREQUAL "0")
    set(${out_ok} TRUE PARENT_SCOPE)
    set(${out_product} "0" PARENT_SCOPE)
    return()
  endif()
  math(EXPR max_left "${GBL_PROFILE_INT64_MAX}/${right}")
  gbl__profile_decimal_greater(exceeds_limit "${left}" "${max_left}")
  if(exceeds_limit)
    set(${out_ok} FALSE PARENT_SCOPE)
    set(${out_product} "" PARENT_SCOPE)
  else()
    math(EXPR result "${left}*${right}")
    set(${out_ok} TRUE PARENT_SCOPE)
    set(${out_product} "${result}" PARENT_SCOPE)
  endif()
endfunction()

# The request-cost table, raw widths/classes, and internal-cycle unit are an
# explicit project policy. Check types and exact values rather than treating
# placeholder values as hardware observations.
function(gbl__profile_bus_policy doc)
  gbl_json_type(pty "${doc}" bus_policy)
  if("${pty}" STREQUAL "MISSING" OR "${pty}" STREQUAL "NULL")
    gbl_finding(profile bus-policy-missing "bus_policy")
    return()
  elseif(NOT "${pty}" STREQUAL "OBJECT")
    gbl_finding(profile bus-policy-value "bus_policy: not an object")
    return()
  endif()

  string(JSON plen LENGTH "${doc}" bus_policy)
  if(NOT plen EQUAL 6)
    gbl_finding(profile bus-policy-value "bus_policy: field count")
  endif()
  gbl__profile_policy_source("${doc}" bus_policy "${GBL_PROFILE_BUS_CITE}" bus-policy-source)

  gbl_json_type(rty "${doc}" bus_policy request_cycles)
  if(NOT "${rty}" STREQUAL "OBJECT")
    gbl_finding(profile bus-policy-value "bus_policy.request_cycles: not an object")
  else()
    string(JSON rlen LENGTH "${doc}" bus_policy request_cycles)
    list(LENGTH GBL_PROFILE_BUS_REQUEST_KEYS rcount)
    if(NOT rlen EQUAL rcount)
      gbl_finding(profile bus-policy-value "bus_policy.request_cycles: field count")
    endif()
    foreach(k IN LISTS GBL_PROFILE_BUS_REQUEST_KEYS)
      gbl_json_type(vty "${doc}" bus_policy request_cycles ${k})
      if(k STREQUAL "rom")
        gbl_json_string(value "${doc}" bus_policy request_cycles rom)
        if(NOT "${vty}" STREQUAL "STRING" OR
           NOT "${value}" STREQUAL "contextual-waitcnt-halfword-beats")
          gbl_finding(profile bus-policy-value
            "bus_policy.request_cycles.rom: expected contextual WAITCNT beat formula")
        endif()
      else()
        if(NOT "${vty}" STREQUAL "NUMBER")
          gbl_finding(profile bus-policy-value "bus_policy.request_cycles.${k}: expected 1")
        else()
          string(JSON value GET "${doc}" bus_policy request_cycles ${k})
          if(NOT "${value}" STREQUAL "1")
            gbl_finding(profile bus-policy-value "bus_policy.request_cycles.${k}: expected 1")
          endif()
        endif()
      endif()
    endforeach()
  endif()

  gbl_json_type(wty "${doc}" bus_policy widths_bytes)
  if(NOT "${wty}" STREQUAL "ARRAY")
    gbl_finding(profile bus-policy-value "bus_policy.widths_bytes: not an array")
  else()
    string(JSON wlen LENGTH "${doc}" bus_policy widths_bytes)
    if(NOT wlen EQUAL 3)
      gbl_finding(profile bus-policy-value "bus_policy.widths_bytes: expected [1, 2, 4]")
    else()
      foreach(i RANGE 0 2)
        gbl_json_type(vty "${doc}" bus_policy widths_bytes ${i})
        string(JSON value GET "${doc}" bus_policy widths_bytes ${i})
        if(i EQUAL 0)
          set(expected 1)
        elseif(i EQUAL 1)
          set(expected 2)
        else()
          set(expected 4)
        endif()
        if(NOT "${vty}" STREQUAL "NUMBER" OR NOT "${value}" STREQUAL "${expected}")
          gbl_finding(profile bus-policy-value "bus_policy.widths_bytes: expected [1, 2, 4]")
        endif()
      endforeach()
    endif()
  endif()

  gbl_json_type(aty "${doc}" bus_policy access_classes)
  if(NOT "${aty}" STREQUAL "ARRAY")
    gbl_finding(profile bus-policy-value "bus_policy.access_classes: not an array")
  else()
    string(JSON alen LENGTH "${doc}" bus_policy access_classes)
    if(NOT alen EQUAL 2)
      gbl_finding(profile bus-policy-value "bus_policy.access_classes: expected [nonsequential, sequential]")
    else()
      string(JSON first GET "${doc}" bus_policy access_classes 0)
      string(JSON second GET "${doc}" bus_policy access_classes 1)
      if(NOT "${first}" STREQUAL "nonsequential" OR NOT "${second}" STREQUAL "sequential")
        gbl_finding(profile bus-policy-value "bus_policy.access_classes: expected [nonsequential, sequential]")
      endif()
    endif()
  endif()

  gbl_json_type(ity "${doc}" bus_policy internal_cycle_unit)
  if(NOT "${ity}" STREQUAL "NUMBER")
    gbl_finding(profile bus-policy-value "bus_policy.internal_cycle_unit: expected 1")
  else()
    string(JSON unit GET "${doc}" bus_policy internal_cycle_unit)
    if(NOT "${unit}" STREQUAL "1")
      gbl_finding(profile bus-policy-value "bus_policy.internal_cycle_unit: expected 1")
    endif()
  endif()
endfunction()

# WAITCNT values are quoted from the manual, while register reset/masking and
# the secondary 128 KiB boundary rule retain their explicit project provenance.
function(gbl__profile_waitcnt_policy doc)
  gbl_json_type(policy_type "${doc}" waitcnt_policy)
  if(NOT "${policy_type}" STREQUAL "OBJECT")
    gbl_finding(profile waitcnt-policy "waitcnt_policy: missing or not an object")
    return()
  endif()
  string(JSON policy_fields LENGTH "${doc}" waitcnt_policy)
  if(NOT policy_fields EQUAL 7)
    gbl_finding(profile waitcnt-policy "waitcnt_policy: expected 7 fields")
  endif()
  foreach(pair IN ITEMS "address;0x04000204" "retained_mask;0x5FFF" "reset_value;0x0000")
    list(GET pair 0 field)
    list(GET pair 1 expected)
    gbl_json_string(value "${doc}" waitcnt_policy register ${field})
    if(NOT "${value}" STREQUAL "${expected}")
      gbl_finding(profile waitcnt-policy "waitcnt_policy.register.${field}: expected ${expected}")
    endif()
  endforeach()
  gbl_json_string(classification "${doc}" waitcnt_policy register classification)
  gbl_json_string(citation "${doc}" waitcnt_policy register cite)
  if(NOT "${classification}" STREQUAL "project-selected" OR
     NOT "${citation}" STREQUAL "${GBL_PROFILE_WAITCNT_REG_CITE}")
    gbl_finding(profile waitcnt-policy "WAITCNT reset/mask must remain explicit project policy")
  endif()

  gbl_json_length(window_count "${doc}" waitcnt_policy windows)
  if(NOT window_count EQUAL 3)
    gbl_finding(profile waitcnt-policy "expected three WAITCNT Game Pak windows")
  endif()
  set(window_ids)
  if(window_count GREATER 0)
    math(EXPR last_window "${window_count}-1")
    foreach(i RANGE ${last_window})
      gbl_json_string(id "${doc}" waitcnt_policy windows ${i} id)
      gbl_json_string(classification "${doc}" waitcnt_policy windows ${i} classification)
      gbl_json_string(citation "${doc}" waitcnt_policy windows ${i} cite)
      if("${id}" IN_LIST window_ids)
        gbl_finding(profile waitcnt-policy "duplicate window ${id}")
      endif()
      list(APPEND window_ids "${id}")
      if(NOT "${id}" IN_LIST GBL_PROFILE_WAITCNT_WINDOWS OR
         NOT "${classification}" STREQUAL "source-backed" OR
         NOT "${citation}" STREQUAL "${GBL_PROFILE_TIMING_NINTENDO_CITE}")
        gbl_finding(profile waitcnt-policy "${id}: expected source-backed manual WAITCNT facts")
      endif()
    endforeach()
  endif()
  foreach(id IN ITEMS ws0 ws1 ws2)
    if(NOT "${id}" IN_LIST window_ids)
      gbl_finding(profile waitcnt-policy "missing WAITCNT window ${id}")
    endif()
  endforeach()

  gbl_json_string(formula "${doc}" waitcnt_policy beat_policy cycle_formula)
  gbl_json_string(beat_class "${doc}" waitcnt_policy beat_policy classification)
  if(NOT "${formula}" STREQUAL "${GBL_PROFILE_TIMING_ROM_FORMULA}" OR
     NOT "${beat_class}" STREQUAL "source-backed")
    gbl_finding(profile waitcnt-policy "beat policy must preserve the contextual halfword formula")
  endif()
  gbl_json_string(boundary_rule "${doc}" waitcnt_policy boundary rule)
  gbl_json_string(boundary_class "${doc}" waitcnt_policy boundary classification)
  gbl_json_string(boundary_status "${doc}" waitcnt_policy boundary hardware_qualification)
  gbl_json_string(boundary_provenance "${doc}" waitcnt_policy boundary provenance)
  if(NOT "${boundary_class}" STREQUAL "secondary-community" OR
     NOT "${boundary_status}" STREQUAL "UNKNOWN" OR
     NOT "${boundary_provenance}" STREQUAL "${GBL_PROFILE_WAITCNT_BOUNDARY_PROVENANCE}" OR
     NOT "${boundary_rule}" STREQUAL "address-aligned beat at a 128 KiB boundary forces effective nonsequential timing")
    gbl_finding(profile waitcnt-policy "128 KiB boundary must remain secondary and UNKNOWN")
  endif()
  gbl_json_string(prefetch_execution "${doc}" waitcnt_policy prefetch execution)
  gbl_json_string(prefetch_status "${doc}" waitcnt_policy prefetch hardware_qualification)
  if(NOT "${prefetch_execution}" STREQUAL "unmodeled" OR NOT "${prefetch_status}" STREQUAL "UNKNOWN")
    gbl_finding(profile waitcnt-policy "prefetch execution must remain unmodeled and UNKNOWN")
  endif()
  gbl_json_string(qualification "${doc}" waitcnt_policy hardware_qualification)
  if(NOT "${qualification}" STREQUAL "UNKNOWN")
    gbl_finding(profile waitcnt-policy "WAITCNT hardware qualification must remain UNKNOWN")
  endif()
endfunction()

# Keep the manual-derived register/transfer facts distinct from selected
# startup/arbitration/latch policies and from deferred trigger producers.
function(gbl__profile_dma_policy doc)
  gbl_json_type(policy_type "${doc}" dma_policy)
  if(NOT "${policy_type}" STREQUAL "OBJECT")
    gbl_finding(profile dma-policy "dma_policy: missing or not an object")
    return()
  endif()
  string(JSON policy_fields LENGTH "${doc}" dma_policy)
  if(NOT policy_fields EQUAL 9)
    gbl_finding(profile dma-policy "dma_policy: expected 9 fields")
  endif()
  gbl_json_string(base "${doc}" dma_policy registers base)
  gbl_json_string(last "${doc}" dma_policy registers last)
  gbl_json_string(control_mask "${doc}" dma_policy registers control_mask)
  gbl_json_string(reg_class "${doc}" dma_policy registers classification)
  gbl_json_string(reg_cite "${doc}" dma_policy registers cite)
  if(NOT "${base}" STREQUAL "0x040000B0" OR NOT "${last}" STREQUAL "0x040000DF" OR
     NOT "${control_mask}" STREQUAL "0xFFE0" OR NOT "${reg_class}" STREQUAL "source-backed" OR
     NOT "${reg_cite}" STREQUAL "${GBL_PROFILE_DMA_CITE}")
    gbl_finding(profile dma-policy "DMA register facts must retain their manual source")
  endif()
  gbl_json_string(facts_class "${doc}" dma_policy source_facts classification)
  gbl_json_string(facts_cite "${doc}" dma_policy source_facts cite)
  if(NOT "${facts_class}" STREQUAL "source-backed" OR NOT "${facts_cite}" STREQUAL "${GBL_PROFILE_DMA_CITE}")
    gbl_finding(profile dma-policy "DMA masks/count/width facts must retain their manual source")
  endif()
  gbl_json_string(trigger_class "${doc}" dma_policy trigger_policy classification)
  gbl_json_string(trigger_cite "${doc}" dma_policy trigger_policy cite)
  gbl_json_string(trigger_refusal "${doc}" dma_policy trigger_policy channel0_mode3)
  gbl_json_string(trigger_deferred "${doc}" dma_policy trigger_policy deferred_behavior)
  if(NOT "${trigger_class}" STREQUAL "project-selected" OR
     NOT "${trigger_cite}" STREQUAL "${GBL_PROFILE_DMA_TRIGGER_CITE}" OR
     NOT "${trigger_refusal}" STREQUAL "refused" OR
     NOT "${trigger_deferred}" STREQUAL "${GBL_PROFILE_DMA_TRIGGER_DEFERRED}")
    gbl_finding(profile dma-policy "trigger producer and refusal policy is missing or stale")
  endif()
  gbl_json_type(startup_cycles_type "${doc}" dma_policy startup_policy master_cycles)
  set(startup_cycles "")
  if("${startup_cycles_type}" STREQUAL "NUMBER")
    string(JSON startup_cycles GET "${doc}" dma_policy startup_policy master_cycles)
  endif()
  gbl_json_string(startup_class "${doc}" dma_policy startup_policy classification)
  gbl_json_string(startup_status "${doc}" dma_policy startup_policy hardware_qualification)
  gbl_json_string(startup_cite "${doc}" dma_policy startup_policy cite)
  if(NOT "${startup_cycles}" STREQUAL "2" OR NOT "${startup_class}" STREQUAL "project-selected" OR
     NOT "${startup_status}" STREQUAL "UNKNOWN" OR NOT "${startup_cite}" STREQUAL "${GBL_PROFILE_DMA_STARTUP_CITE}")
    gbl_finding(profile dma-policy "DMA startup conversion must remain project-selected and UNKNOWN")
  endif()
  gbl_json_string(arb_class "${doc}" dma_policy arbitration_policy classification)
  gbl_json_string(arb_status "${doc}" dma_policy arbitration_policy hardware_qualification)
  gbl_json_string(arb_boundary "${doc}" dma_policy arbitration_policy unit_boundary)
  gbl_json_string(arb_finer "${doc}" dma_policy arbitration_policy finer_boundary)
  gbl_json_string(arb_cite "${doc}" dma_policy arbitration_policy cite)
  if(NOT "${arb_class}" STREQUAL "project-selected" OR NOT "${arb_status}" STREQUAL "UNKNOWN" OR
     NOT "${arb_boundary}" STREQUAL "one source access plus one destination access" OR
     NOT "${arb_finer}" STREQUAL "unmodeled" OR NOT "${arb_cite}" STREQUAL "${GBL_PROFILE_DMA_ARBITRATION_CITE}")
    gbl_finding(profile dma-policy "DMA arbitration policy must remain project-selected and UNKNOWN")
  endif()
  gbl_json_string(latch_class "${doc}" dma_policy latch_policy result_classification)
  gbl_json_string(latch_dispute "${doc}" dma_policy latch_policy dispute_id)
  gbl_json_string(latch_status "${doc}" dma_policy latch_policy dispute_status)
  gbl_json_string(latch_hw "${doc}" dma_policy latch_policy hardware_qualification)
  gbl_json_string(latch_cite "${doc}" dma_policy latch_policy cite)
  if(NOT "${latch_class}" STREQUAL "disputed" OR NOT "${latch_dispute}" STREQUAL "disp-dma-latch" OR
     NOT "${latch_status}" STREQUAL "open" OR NOT "${latch_hw}" STREQUAL "UNKNOWN" OR
     NOT "${latch_cite}" STREQUAL "${GBL_PROFILE_DMA_LATCH_CITE}")
    gbl_finding(profile dma-policy "DMA latch dispute must remain open, disputed, and UNKNOWN")
  endif()
  gbl_json_string(qualification "${doc}" dma_policy hardware_qualification)
  if(NOT "${qualification}" STREQUAL "UNKNOWN")
    gbl_finding(profile dma-policy "DMA hardware qualification must remain UNKNOWN")
  endif()
endfunction()

# The executed bus costs and their hardware interpretation are deliberately
# separate. The first is a complete project model; the latter remains UNKNOWN
# wherever this profile has not sourced a complete region/width/sequence rule.
function(gbl__profile_timing_model doc)
  gbl_json_type(mty "${doc}" timing_model)
  if(NOT "${mty}" STREQUAL "OBJECT")
    gbl_finding(profile timing-model-missing "timing_model: missing or not an object")
    return()
  endif()
  string(JSON model_fields LENGTH "${doc}" timing_model)
  if(NOT model_fields EQUAL 7)
    gbl_finding(profile timing-model-value "timing_model: expected 7 fields")
  endif()
  gbl_json_string(model_id "${doc}" timing_model id)
  if(NOT "${model_id}" STREQUAL "${GBL_PROFILE_TIMING_MODEL_ID}")
    gbl_finding(profile timing-model-value "timing_model.id: expected ${GBL_PROFILE_TIMING_MODEL_ID}")
  endif()
  gbl_json_string(qualification "${doc}" timing_model qualification)
  if(NOT "${qualification}" STREQUAL "project-model-only")
    gbl_finding(profile timing-model-qualification "timing_model.qualification must be project-model-only")
  endif()

  set(expected_keys)
  foreach(direction IN LISTS GBL_PROFILE_BUS_DIRECTIONS)
    foreach(region IN LISTS GBL_PROFILE_BUS_REGIONS)
      foreach(width IN LISTS GBL_PROFILE_BUS_WIDTHS)
        foreach(sequence IN LISTS GBL_PROFILE_BUS_SEQUENCE_CLASSES)
          if(sequence STREQUAL "nonsequential")
            set(sequence_code n)
          else()
            set(sequence_code s)
          endif()
          foreach(role IN LISTS GBL_PROFILE_BUS_ROLES)
            if(direction STREQUAL "write" AND role STREQUAL "fetch")
              continue()
            endif()
            list(APPEND expected_keys "${direction}/${region}/${width}/${sequence_code}/${role}")
          endforeach()
        endforeach()
      endforeach()
    endforeach()
  endforeach()
  list(LENGTH expected_keys expected_count)

  gbl_json_length(row_count "${doc}" timing_model bus_access_costs)
  if(NOT row_count EQUAL expected_count)
    gbl_finding(profile timing-model-row-count
      "timing_model.bus_access_costs: expected ${expected_count} rows, found ${row_count}")
  endif()
  set(seen_keys)
  if(row_count GREATER 0)
    math(EXPR last_row "${row_count}-1")
    foreach(i RANGE ${last_row})
      gbl_json_string(key "${doc}" timing_model bus_access_costs ${i} key)
      gbl_json_string(direction "${doc}" timing_model bus_access_costs ${i} direction)
      gbl_json_string(region "${doc}" timing_model bus_access_costs ${i} region)
      gbl_json_string(sequence "${doc}" timing_model bus_access_costs ${i} sequence)
      gbl_json_string(role "${doc}" timing_model bus_access_costs ${i} role)
      gbl_json_type(width_type "${doc}" timing_model bus_access_costs ${i} width_bytes)
      set(width invalid)
      if("${width_type}" STREQUAL "NUMBER")
        string(JSON width GET "${doc}" timing_model bus_access_costs ${i} width_bytes)
      else()
        gbl_finding(profile timing-model-key "bus_access_costs[${i}]: width_bytes must be an integer")
      endif()
      if(sequence STREQUAL "nonsequential")
        set(sequence_code n)
      elseif(sequence STREQUAL "sequential")
        set(sequence_code s)
      else()
        set(sequence_code invalid)
      endif()
      set(expected_key "${direction}/${region}/${width}/${sequence_code}/${role}")
      if(NOT "${key}" STREQUAL "${expected_key}")
        gbl_finding(profile timing-model-key "bus_access_costs[${i}]: key does not match dimensions")
      endif()
      if("${key}" IN_LIST seen_keys)
        gbl_finding(profile timing-model-duplicate-key "${key}")
      endif()
      list(APPEND seen_keys "${key}")
      if(NOT "${key}" IN_LIST expected_keys)
        gbl_finding(profile timing-model-key "unsupported access key ${key}")
      endif()

      gbl_json_type(execution_type "${doc}" timing_model bus_access_costs ${i} execution)
      set(execution_cost "")
      if(NOT "${execution_type}" STREQUAL "OBJECT")
        gbl_finding(profile timing-model-execution "${key}: execution policy missing")
      else()
        string(JSON execution_fields LENGTH "${doc}" timing_model bus_access_costs ${i} execution)
        gbl_json_string(execution_class "${doc}" timing_model bus_access_costs ${i} execution classification)
        gbl_json_type(execution_cost_type "${doc}" timing_model bus_access_costs ${i} execution cost_cycles)
        if("${execution_cost_type}" STREQUAL "NUMBER")
          string(JSON execution_cost GET "${doc}" timing_model bus_access_costs ${i} execution cost_cycles)
        endif()
        gbl_json_string(execution_cite "${doc}" timing_model bus_access_costs ${i} execution citation)
        if(region STREQUAL "rom")
          gbl_json_string(execution_formula "${doc}" timing_model bus_access_costs ${i} execution formula)
          if(NOT execution_fields EQUAL 4 OR
             NOT "${execution_class}" STREQUAL "source-backed" OR
             NOT "${execution_cost_type}" STREQUAL "NULL" OR
             NOT "${execution_formula}" STREQUAL "${GBL_PROFILE_TIMING_ROM_FORMULA}" OR
             NOT "${execution_cite}" STREQUAL "${GBL_PROFILE_TIMING_ROM_EXECUTION_CITE}")
            gbl_finding(profile timing-model-execution
              "${key}: expected contextual WAITCNT source-backed beat formula")
          endif()
        elseif(NOT execution_fields EQUAL 3 OR
           NOT "${execution_class}" STREQUAL "project-selected" OR
           NOT "${execution_cost_type}" STREQUAL "NUMBER" OR
           NOT "${execution_cost}" STREQUAL "1" OR
           NOT "${execution_cite}" STREQUAL "${GBL_PROFILE_TIMING_EXECUTION_CITE}")
          gbl_finding(profile timing-model-execution "${key}: expected selected non-ROM one-cycle execution row")
        endif()
      endif()

      gbl_json_type(hardware_type "${doc}" timing_model bus_access_costs ${i} hardware_cost)
      if(NOT "${hardware_type}" STREQUAL "OBJECT")
        gbl_finding(profile timing-model-hardware-cost "${key}: hardware-cost provenance missing")
      else()
        string(JSON hardware_fields LENGTH "${doc}" timing_model bus_access_costs ${i} hardware_cost)
        if(NOT hardware_fields EQUAL 4)
          gbl_finding(profile timing-model-hardware-cost "${key}: hardware-cost field count")
        endif()
        gbl_json_string(hardware_class "${doc}" timing_model bus_access_costs ${i} hardware_cost classification)
        gbl_json_type(hardware_cost_type "${doc}" timing_model bus_access_costs ${i} hardware_cost cost_cycles)
        gbl_json_string(hardware_cite "${doc}" timing_model bus_access_costs ${i} hardware_cost citation)
        if(NOT "${hardware_class}" STREQUAL "unmodeled" OR
           NOT "${hardware_cost_type}" STREQUAL "NULL" OR
           "${hardware_cite}" STREQUAL "<missing>" OR "${hardware_cite}" STREQUAL "")
          gbl_finding(profile timing-model-hardware-cost "${key}: hardware cost must remain unmodeled with null cost and citation")
        endif()
        if(region STREQUAL "rom")
          set(expected_hardware_cite "${GBL_PROFILE_TIMING_ROM_HARDWARE_CITE}")
        else()
          set(expected_hardware_cite "${GBL_PROFILE_TIMING_OTHER_HARDWARE_CITE}")
        endif()
        if(NOT "${hardware_cite}" STREQUAL "${expected_hardware_cite}")
          gbl_finding(profile timing-model-hardware-cost "${key}: hardware citation is inflated or stale")
        endif()
        gbl_json_length(hardware_gap_count "${doc}" timing_model bus_access_costs ${i} hardware_cost unmodeled_ids)
        set(hardware_gaps)
        if(hardware_gap_count GREATER 0)
          math(EXPR last_gap "${hardware_gap_count}-1")
          foreach(g RANGE ${last_gap})
            gbl_json_string(gap "${doc}" timing_model bus_access_costs ${i} hardware_cost unmodeled_ids ${g})
            list(APPEND hardware_gaps "${gap}")
          endforeach()
        endif()
        set(required_gaps bus-costs)
        if(region STREQUAL "rom")
          list(APPEND required_gaps waitcnt)
          if(role STREQUAL "fetch")
            list(APPEND required_gaps prefetch)
          endif()
        endif()
        foreach(gap IN LISTS required_gaps)
          if(NOT "${gap}" IN_LIST hardware_gaps)
            gbl_finding(profile timing-model-hardware-cost "${key}: missing unmodeled ${gap} limit")
          endif()
        endforeach()
        foreach(gap IN LISTS hardware_gaps)
          if(NOT "${gap}" IN_LIST GBL_PROFILE_REQUIRED_UNMODELED)
            gbl_finding(profile timing-model-hardware-cost "${key}: unknown unmodeled id ${gap}")
          endif()
        endforeach()
      endif()

      # bus_policy is a compatibility summary; it may not disagree with any
      # keyed row that the runtime actually executes.
      if("${region}" IN_LIST GBL_PROFILE_BUS_REGIONS AND
         NOT region STREQUAL "rom" AND NOT "${execution_cost}" STREQUAL "")
        gbl_json_type(compatibility_type "${doc}" bus_policy request_cycles ${region})
        if(NOT "${compatibility_type}" STREQUAL "NUMBER")
          gbl_finding(profile timing-model-execution "${key}: bus_policy compatibility row missing")
        else()
          string(JSON compatibility_cost GET "${doc}" bus_policy request_cycles ${region})
          if(NOT "${compatibility_cost}" STREQUAL "${execution_cost}")
          gbl_finding(profile timing-model-execution "${key}: bus_policy compatibility summary disagrees")
          endif()
        endif()
      endif()
    endforeach()
  endif()
  foreach(key IN LISTS expected_keys)
    if(NOT "${key}" IN_LIST seen_keys)
      gbl_finding(profile timing-model-missing-key "${key}")
    endif()
  endforeach()

  gbl_json_length(cpu_count "${doc}" timing_model cpu_composition)
  if(NOT cpu_count EQUAL 4)
    gbl_finding(profile timing-model-cpu-composition "expected 4 separate CPU composition provenance rows")
  endif()
  if(cpu_count GREATER 0)
    math(EXPR last_cpu "${cpu_count}-1")
    foreach(i RANGE ${last_cpu})
      gbl_json_string(id "${doc}" timing_model cpu_composition ${i} id)
      gbl_json_string(classification "${doc}" timing_model cpu_composition ${i} classification)
      gbl_json_string(rule "${doc}" timing_model cpu_composition ${i} rule)
      gbl_json_string(citation "${doc}" timing_model cpu_composition ${i} citation)
      if("${rule}" STREQUAL "<missing>" OR "${citation}" STREQUAL "<missing>")
        gbl_finding(profile timing-model-cpu-composition "${id}: rule and citation are required")
      endif()
      if(i LESS 2)
        if(NOT "${classification}" STREQUAL "source-backed" OR
           NOT "${citation}" STREQUAL "${GBL_PROFILE_TIMING_ARM_CITE}")
          gbl_finding(profile timing-model-cpu-composition "${id}: ARM source-backed rule has inflated or stale provenance")
        endif()
      elseif(i EQUAL 2)
        if(NOT "${id}" STREQUAL "gba-waitcnt-wait-selection" OR
           NOT "${classification}" STREQUAL "source-backed" OR
           NOT "${citation}" STREQUAL "${GBL_PROFILE_TIMING_NINTENDO_CITE}")
          gbl_finding(profile timing-model-cpu-composition "${id}: WAITCNT source rule has inflated or stale provenance")
        endif()
      elseif(NOT "${id}" STREQUAL "gba-prefetch-execution" OR
             NOT "${classification}" STREQUAL "unmodeled")
        gbl_finding(profile timing-model-cpu-composition "${id}: GBA wait mapping must remain unmodeled")
      endif()
    endforeach()
  endif()

  gbl_json_length(edge_count "${doc}" timing_model timer_edge_policies)
  if(NOT edge_count EQUAL 6)
    gbl_finding(profile timing-model-edge "expected six explicit timer edge policies")
  endif()
  set(edge_ids)
  if(edge_count GREATER 0)
    math(EXPR last_edge "${edge_count}-1")
    foreach(i RANGE ${last_edge})
      gbl_json_string(id "${doc}" timing_model timer_edge_policies ${i} id)
      gbl_json_string(classification "${doc}" timing_model timer_edge_policies ${i} classification)
      gbl_json_string(selected_rule "${doc}" timing_model timer_edge_policies ${i} selected_rule)
      gbl_json_string(hardware_status "${doc}" timing_model timer_edge_policies ${i} hardware_qualification)
      gbl_json_string(citation "${doc}" timing_model timer_edge_policies ${i} citation)
      if("${id}" IN_LIST edge_ids)
        gbl_finding(profile timing-model-edge "duplicate edge ${id}")
      endif()
      list(APPEND edge_ids "${id}")
      if(NOT "${classification}" STREQUAL "project-selected" OR
         NOT "${hardware_status}" STREQUAL "UNKNOWN" OR
         "${selected_rule}" STREQUAL "<missing>" OR "${citation}" STREQUAL "<missing>" OR
         NOT "${citation}" STREQUAL "${GBL_PROFILE_TIMING_EDGE_CITE}")
        gbl_finding(profile timing-model-edge "${id}: selected edge and UNKNOWN hardware qualification required")
      endif()
    endforeach()
  endif()
  foreach(id IN ITEMS first-tick-after-enable live-reload enabled-control-write same-cycle-register-access equal-cycle-order timer0-cascade)
    if(NOT "${id}" IN_LIST edge_ids)
      gbl_finding(profile timing-model-edge "missing explicit edge ${id}")
    endif()
  endforeach()

  gbl_json_length(guest_count "${doc}" timing_model guest_path_coverage)
  if(NOT guest_count EQUAL 6)
    gbl_finding(profile timing-model-guest-path "expected six exact guest access shapes")
  endif()
  set(guest_ids)
  if(guest_count GREATER 0)
    math(EXPR last_guest "${guest_count}-1")
    foreach(i RANGE ${last_guest})
      gbl_json_string(id "${doc}" timing_model guest_path_coverage ${i} id)
      gbl_json_string(key "${doc}" timing_model guest_path_coverage ${i} key)
      gbl_json_string(description "${doc}" timing_model guest_path_coverage ${i} description)
      if("${id}" IN_LIST guest_ids)
        gbl_finding(profile timing-model-guest-path "duplicate guest path ${id}")
      endif()
      list(APPEND guest_ids "${id}")
      if(NOT "${key}" IN_LIST seen_keys OR "${description}" STREQUAL "<missing>")
        gbl_finding(profile timing-model-guest-path "${id}: access shape has no unique bus cost row")
      endif()
    endforeach()
  endif()

  gbl_json_length(gap_count "${doc}" timing_model unmodeled_hardware)
  if(NOT gap_count EQUAL 3)
    gbl_finding(profile timing-model-gap "expected bus-costs, waitcnt and prefetch hardware gaps")
  endif()
  set(gap_ids)
  if(gap_count GREATER 0)
    math(EXPR last_gap "${gap_count}-1")
    foreach(i RANGE ${last_gap})
      gbl_json_string(id "${doc}" timing_model unmodeled_hardware ${i} id)
      gbl_json_string(scope "${doc}" timing_model unmodeled_hardware ${i} affected_shapes)
      gbl_json_string(classification "${doc}" timing_model unmodeled_hardware ${i} classification)
      gbl_json_string(status "${doc}" timing_model unmodeled_hardware ${i} hardware_qualification)
      gbl_json_string(citation "${doc}" timing_model unmodeled_hardware ${i} citation)
      if("${id}" IN_LIST gap_ids)
        gbl_finding(profile timing-model-gap "duplicate hardware gap ${id}")
      endif()
      list(APPEND gap_ids "${id}")
      if(NOT "${id}" IN_LIST GBL_PROFILE_REQUIRED_UNMODELED OR
         NOT "${classification}" STREQUAL "unmodeled" OR
         NOT "${status}" STREQUAL "UNKNOWN" OR
         "${scope}" STREQUAL "<missing>" OR "${citation}" STREQUAL "<missing>")
        gbl_finding(profile timing-model-gap "${id}: explicit UNKNOWN hardware gap required")
      endif()
    endforeach()
  endif()
  foreach(id IN ITEMS bus-costs waitcnt prefetch)
    if(NOT "${id}" IN_LIST gap_ids)
      gbl_finding(profile timing-model-gap "missing required unmodeled hardware gap ${id}")
    endif()
  endforeach()
endfunction()

function(gbl__profile_initialization_policy doc)
  gbl_json_type(pty "${doc}" initialization_policy)
  if(NOT "${pty}" STREQUAL "OBJECT")
    gbl_finding(profile init-policy "initialization_policy: missing or not an object")
    return()
  endif()
  string(JSON plen LENGTH "${doc}" initialization_policy)
  if(NOT plen EQUAL 6)
    gbl_finding(profile init-policy "initialization_policy: field count")
  endif()
  gbl__profile_policy_source("${doc}" initialization_policy "${GBL_PROFILE_INIT_CITE}" init-policy)
  foreach(k IN ITEMS memory_fill register_fill master_cycles open_bus)
    gbl_json_type(vty "${doc}" initialization_policy ${k})
    if(NOT "${vty}" STREQUAL "NUMBER")
      gbl_finding(profile init-policy "initialization_policy.${k}: expected 0")
    else()
      string(JSON value GET "${doc}" initialization_policy ${k})
      if(NOT "${value}" STREQUAL "0")
        gbl_finding(profile init-policy "initialization_policy.${k}: expected 0")
      endif()
    endif()
  endforeach()
endfunction()

function(gbl__profile_read_policy doc)
  gbl_json_type(pty "${doc}" read_policy)
  if(NOT "${pty}" STREQUAL "OBJECT")
    gbl_finding(profile read-policy "read_policy: missing or not an object")
    return()
  endif()
  string(JSON plen LENGTH "${doc}" read_policy)
  if(NOT plen EQUAL 7)
    gbl_finding(profile read-policy "read_policy: field count")
  endif()
  gbl__profile_policy_source("${doc}" read_policy "${GBL_PROFILE_READ_CITE}" read-policy)
  foreach(k IN ITEMS absent_bios unsupported_io rom_tail)
    gbl_json_string(value "${doc}" read_policy ${k})
    if(NOT "${value}" STREQUAL "current-open-bus")
      gbl_finding(profile read-policy "read_policy.${k}: expected current-open-bus")
    endif()
  endforeach()
  gbl_json_string(latch "${doc}" read_policy latch_update)
  if(NOT "${latch}" STREQUAL "mapped-read-width-replicated")
    gbl_finding(profile read-policy "read_policy.latch_update: expected mapped-read-width-replicated")
  endif()
  gbl_json_string(write_latch "${doc}" read_policy write_latch)
  if(NOT "${write_latch}" STREQUAL "preserve")
    gbl_finding(profile read-policy "read_policy.write_latch: expected preserve")
  endif()
endfunction()

function(gbl__profile_cpu_policy doc)
  string(JSON cpu_version ERROR_VARIABLE version_error GET "${doc}" profile_version)
  if(NOT "${cpu_version}" MATCHES "^[1-9][0-9]*$" OR cpu_version LESS 5)
    gbl_finding(profile cpu-policy-value "profile_version: expected 5 or later for this CPU contract")
  endif()
  gbl_json_type(pty "${doc}" cpu_policy)
  if(NOT "${pty}" STREQUAL "OBJECT")
    gbl_finding(profile cpu-policy-missing "cpu_policy: missing or not an object")
    return()
  endif()
  string(JSON plen LENGTH "${doc}" cpu_policy)
  if(NOT plen EQUAL 26)
    gbl_finding(profile cpu-policy-value "cpu_policy: field count")
  endif()

  gbl__profile_policy_source("${doc}" cpu_policy "${GBL_PROFILE_CPU_CITE}" cpu-policy-source)
  gbl_json_string(architecture_source "${doc}" cpu_policy architecture_source)
  gbl_json_string(architecture_cite "${doc}" cpu_policy architecture_cite)
  if(NOT "${architecture_source}" STREQUAL "REF" OR
     NOT "${architecture_cite}" STREQUAL "${GBL_PROFILE_CPU_ARCH_CITE}")
    gbl_finding(profile cpu-policy-source "cpu_policy: ARM architecture source/citation")
  endif()

  foreach(pair IN ITEMS
      "entry_address|134217728"
      "cpsr|223"
      "register_bank_fill|0"
      "spsr_fill|0"
      "arm_pc_offset|8"
      "thumb_pc_offset|4"
      "exception_entry_cycles|1")
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 key)
    list(GET fields 1 expected)
    gbl_json_type(vty "${doc}" cpu_policy "${key}")
    if(NOT "${vty}" STREQUAL "NUMBER")
      gbl_finding(profile cpu-policy-value "cpu_policy.${key}: expected ${expected}")
    else()
      string(JSON value GET "${doc}" cpu_policy "${key}")
      if(NOT "${value}" STREQUAL "${expected}")
        gbl_finding(profile cpu-policy-value "cpu_policy.${key}: expected ${expected}")
      endif()
    endif()
  endforeach()

  foreach(pair IN ITEMS
      "user_system_spsr|absent"
      "register_banks|User/System share r8-r14, FIQ has private r8-r14, IRQ/SVC/ABT/UND each have private r13-r14, and FIQ/IRQ/SVC/ABT/UND have SPSRs."
      "pipeline|empty"
      "timing|unit-preflight-uniform-bus-plus-core-internal-work"
      "bios_none_stop|after-entry-before-vector-fetch"
      "execution|single-interpreter"
      "pc_operand_reads|arm-current+8-register-shift-rm-and-store+12-thumb-current+4"
      "internal_work|ddi0029g-6.5-6.6-6.20-cost-table"
      "multiply_flag_policy|preserve-short-c-long-cv"
      "loaded_pc_boundary|align-mask-wrap-refill"
      "unpredictable_operands|named-stop"
      "extension_encoding|later-architecture-stop"
      "address_arithmetic|data-modulo32-static-fetch-checked")
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 key)
    list(GET fields 1 expected)
    gbl_json_string(value "${doc}" cpu_policy "${key}")
    if(NOT "${value}" STREQUAL "${expected}")
      gbl_finding(profile cpu-policy-value "cpu_policy.${key}: expected ${expected}")
    endif()
  endforeach()

  foreach(spec IN ITEMS "supported_opcode_operands|GBL_PROFILE_CPU_SUPPORTED"
                        "excluded_opcode_operands|GBL_PROFILE_CPU_EXCLUDED")
    string(REPLACE "|" ";" fields "${spec}")
    list(GET fields 0 key)
    list(GET fields 1 expected_var)
    gbl_json_type(aty "${doc}" cpu_policy "${key}")
    if(NOT "${aty}" STREQUAL "ARRAY")
      gbl_finding(profile cpu-policy-value "cpu_policy.${key}: expected array")
      continue()
    endif()
    string(JSON actual_length LENGTH "${doc}" cpu_policy "${key}")
    list(LENGTH ${expected_var} expected_length)
    if(NOT actual_length EQUAL expected_length)
      gbl_finding(profile cpu-policy-value "cpu_policy.${key}: expected ${expected_length} rows, found ${actual_length}")
      continue()
    endif()
    math(EXPR last "${expected_length}-1")
    foreach(i RANGE ${last})
      string(JSON actual GET "${doc}" cpu_policy "${key}" ${i})
      list(GET ${expected_var} ${i} expected)
      if(NOT "${actual}" STREQUAL "${expected}")
        gbl_finding(profile cpu-policy-value "cpu_policy.${key}[${i}]: unexpected opcode/operand scope")
      endif()
    endforeach()
  endforeach()
endfunction()

function(gbl__profile_irq_input_power_policy doc)
  string(JSON policy_version ERROR_VARIABLE version_error GET
    "${doc}" profile_version)
  if(NOT "${policy_version}" MATCHES "^[1-9][0-9]*$" OR
      policy_version LESS 7)
    gbl_finding(profile irq-policy-value
      "profile_version: expected 7 or later for Phase 8 policy")
  endif()

  gbl_json_type(policy_type "${doc}" irq_input_power_policy)
  if(NOT "${policy_type}" STREQUAL "OBJECT")
    gbl_finding(profile irq-policy-missing
      "irq_input_power_policy: missing or not an object")
    return()
  endif()
  string(JSON policy_length LENGTH "${doc}" irq_input_power_policy)
  if(NOT policy_length EQUAL 31)
    gbl_finding(profile irq-policy-value
      "irq_input_power_policy: field count")
  endif()
  gbl__profile_policy_source("${doc}" irq_input_power_policy
    "${GBL_PROFILE_IRQ_POLICY_CITE}" irq-policy-source)

  gbl_json_type(source_facts_type "${doc}" irq_input_power_policy source_facts)
  if(NOT "${source_facts_type}" STREQUAL "OBJECT")
    gbl_finding(profile irq-policy-source
      "irq_input_power_policy.source_facts: expected object")
  else()
    string(JSON source_facts_length LENGTH "${doc}"
      irq_input_power_policy source_facts)
    if(NOT source_facts_length EQUAL 2)
      gbl_finding(profile irq-policy-source
        "irq_input_power_policy.source_facts: expected two cited facts")
    endif()
    foreach(pair IN ITEMS
        "arm_exception_entry|${GBL_PROFILE_IRQ_ARM_CITE}"
        "gba_devices_and_power|${GBL_PROFILE_IRQ_GBA_CITE}")
      string(REPLACE "|" ";" fields "${pair}")
      list(GET fields 0 key)
      list(GET fields 1 expected_cite)
      gbl_json_type(fact_type "${doc}" irq_input_power_policy source_facts "${key}")
      gbl_json_string(fact_source "${doc}" irq_input_power_policy source_facts "${key}" source)
      gbl_json_string(fact_cite "${doc}" irq_input_power_policy source_facts "${key}" cite)
      if(NOT "${fact_type}" STREQUAL "OBJECT" OR
          NOT "${fact_source}" STREQUAL "REF" OR
          NOT "${fact_cite}" STREQUAL "${expected_cite}")
        gbl_finding(profile irq-policy-source
          "irq_input_power_policy.source_facts.${key}: expected cited REF source")
      endif()
    endforeach()
  endif()

  foreach(pair IN ITEMS
      "if_ie_mask|16383"
      "ime_mask|1"
      "cpsr_i_mask|128"
      "timer0_if_bit|8"
      "keypad_if_bit|4096"
      "keyinput_mask|1023"
      "keycnt_mask|50175"
      "entry_internal_cycles|1"
      "keypad_event_priority|4"
      "input_event_capacity|256"
      "stop_cause_code|3"
      "deadlock_cause_code|4"
      "timeout_reason_code|2")
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 key)
    list(GET fields 1 expected)
    gbl_json_type(value_type "${doc}" irq_input_power_policy "${key}")
    if(NOT "${value_type}" STREQUAL "NUMBER")
      gbl_finding(profile irq-policy-value
        "irq_input_power_policy.${key}: expected ${expected}")
    else()
      string(JSON value GET "${doc}" irq_input_power_policy "${key}")
      if(NOT "${value}" STREQUAL "${expected}")
        gbl_finding(profile irq-policy-value
          "irq_input_power_policy.${key}: expected ${expected}")
      endif()
    endif()
  endforeach()

  foreach(pair IN ITEMS
      "sample_boundary|completed-instruction-effects-and-events-before-next-instruction"
      "initial_boundary|after-refill-before-first-instruction"
      "no_bios_irq_stop|after-entry-before-vector-fetch"
      "key_polarity|active-low"
      "keycnt_effective_edge|false-to-true"
      "if_ack_while_held|stays-clear-until-condition-falls-then-rises"
      "halt_wake_condition|IF-and-IE"
      "power_transition_cost|internal-only"
      "stop_policy|terminal-guest-stop-no-bios-wake"
      "deadlock_policy|distinct-guest-stop"
      "timeout_policy|cycle-budget-timeout"
      "qualification|project-model-only")
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 key)
    list(GET fields 1 expected)
    gbl_json_string(value "${doc}" irq_input_power_policy "${key}")
    if(NOT "${value}" STREQUAL "${expected}")
      gbl_finding(profile irq-policy-value
        "irq_input_power_policy.${key}: expected ${expected}")
    endif()
  endforeach()

  gbl_json_type(empty_selection_type "${doc}" irq_input_power_policy keycnt_empty_selection)
  string(JSON empty_selection_value ERROR_VARIABLE empty_selection_error
    GET "${doc}" irq_input_power_policy keycnt_empty_selection)
  set(empty_selection_is_false FALSE)
  if("${empty_selection_value}" STREQUAL "false" OR
      "${empty_selection_value}" STREQUAL "OFF")
    set(empty_selection_is_false TRUE)
  endif()
  if(NOT "${empty_selection_type}" STREQUAL "BOOLEAN" OR
      NOT empty_selection_is_false)
    gbl_finding(profile irq-policy-value
      "irq_input_power_policy.keycnt_empty_selection: expected false")
  endif()

  foreach(spec IN ITEMS "timer_event_priorities|4" "halt_wake_mask_independent|2")
    string(REPLACE "|" ";" fields "${spec}")
    list(GET fields 0 key)
    list(GET fields 1 expected_count)
    gbl_json_type(array_type "${doc}" irq_input_power_policy "${key}")
    if(NOT "${array_type}" STREQUAL "ARRAY")
      gbl_finding(profile irq-policy-value
        "irq_input_power_policy.${key}: expected array")
      continue()
    endif()
    string(JSON actual_count LENGTH "${doc}" irq_input_power_policy "${key}")
    if(NOT actual_count EQUAL expected_count)
      gbl_finding(profile irq-policy-value
        "irq_input_power_policy.${key}: expected ${expected_count} rows")
      continue()
    endif()
    if(key STREQUAL "timer_event_priorities")
      set(expected_values 0 1 2 3)
    else()
      set(expected_values IME CPSR.I)
    endif()
    math(EXPR last "${expected_count}-1")
    foreach(index RANGE ${last})
      string(JSON actual GET "${doc}" irq_input_power_policy "${key}" ${index})
      list(GET expected_values ${index} expected)
      if(NOT "${actual}" STREQUAL "${expected}")
        gbl_finding(profile irq-policy-value
          "irq_input_power_policy.${key}[${index}]: expected ${expected}")
      endif()
    endforeach()
  endforeach()
endfunction()

# gbl__profile_dispute(<doc> <where> <key>...): when a `dispute` member exists at
# <key>..., it must name a registry id (R_IDS in the caller's scope).
function(gbl__profile_dispute doc where)
  gbl_json_type(dty "${doc}" ${ARGN} dispute)
  if("${dty}" STREQUAL "MISSING")
    return()
  endif()
  gbl_json_string(did "${doc}" ${ARGN} dispute)
  if("${did}" STREQUAL "<missing>")
    gbl_finding(profile dispute-unknown "${where}: dispute is not a non-empty string")
  elseif(NOT "${did}" IN_LIST R_IDS)
    gbl_finding(profile dispute-unknown "${where}: ${did}")
  endif()
endfunction()

# gbl__profile_timing(<doc>): the eleven timing entries. Sets T_<key> (integer
# text, or empty when unavailable) in the caller's scope, then checks the
# arithmetic invariants over the values that are available.
function(gbl__profile_timing doc)
  gbl_json_type(tty "${doc}" timing)
  if(NOT "${tty}" STREQUAL "OBJECT")
    return()
  endif()
  foreach(k IN LISTS GBL_PROFILE_TIMING_KEYS)
    set(T_${k} "")
    gbl_json_type(ety "${doc}" timing ${k})
    if(NOT "${ety}" STREQUAL "OBJECT")
      gbl_finding(profile missing-field "timing.${k}")
      continue()
    endif()

    gbl_json_string(src "${doc}" timing ${k} source)
    set(label_ok FALSE)
    if("${src}" STREQUAL "<missing>")
      gbl_finding(profile missing-source "timing.${k}: source")
    elseif("${src}" IN_LIST V_SOURCE_LABELS)
      set(label_ok TRUE)
    else()
      gbl_finding(profile bad-source-label "timing.${k}")
    endif()
    if(NOT "${src}" STREQUAL "UNK" AND NOT "${src}" STREQUAL "<missing>")
      gbl_json_string(cite "${doc}" timing ${k} cite)
      if("${cite}" STREQUAL "<missing>")
        gbl_finding(profile missing-source "timing.${k}: cite")
      endif()
    endif()

    gbl_json_type(vty "${doc}" timing ${k} value)
    if("${k}" STREQUAL "hblank_flag_edge_cycle")
      if("${vty}" STREQUAL "MISSING")
        gbl_finding(profile missing-field "timing.${k}.value")
      elseif(NOT "${vty}" STREQUAL "NULL")
        gbl_finding(profile hblank-edge-hardcoded "timing.${k}.value")
      endif()
      if(NOT "${src}" STREQUAL "<missing>" AND label_ok AND NOT "${src}" STREQUAL "UNK" AND "${vty}" STREQUAL "NULL")
        gbl_finding(profile bad-source-label "timing.${k}: null value under ${src}")
      endif()
      gbl__profile_dispute("${doc}" "timing.${k}" timing ${k})
      gbl_json_type(dty "${doc}" timing ${k} dispute)
      if("${dty}" STREQUAL "MISSING")
        gbl_finding(profile dispute-unknown "timing.${k}: no dispute")
      endif()
      continue()
    endif()

    if("${vty}" STREQUAL "NULL")
      if(label_ok AND NOT "${src}" STREQUAL "UNK")
        gbl_finding(profile bad-source-label "timing.${k}: null value under ${src}")
      endif()
    elseif("${vty}" STREQUAL "NUMBER")
      string(JSON vtext GET "${doc}" timing ${k} value)
      if("${vtext}" MATCHES "^(0|[1-9][0-9]*)$")
        gbl__profile_int64_in_range(in_range "${vtext}")
        if(in_range)
          set(T_${k} "${vtext}")
        else()
          gbl_finding(profile timing-arithmetic "timing.${k}: exceeds signed arithmetic range")
        endif()
      else()
        gbl_finding(profile timing-arithmetic "timing.${k}: not an integer")
      endif()
    else()
      gbl_finding(profile missing-field "timing.${k}.value")
    endif()
    gbl__profile_dispute("${doc}" "timing.${k}" timing ${k})
  endforeach()

  # Invariants, each only over operands that are available integers.
  if(NOT "${T_dots_visible}" STREQUAL "" AND NOT "${T_dots_hblank}" STREQUAL ""
      AND NOT "${T_dots_per_line}" STREQUAL "")
    gbl__profile_safe_sum(sum_ok sum "${T_dots_visible}" "${T_dots_hblank}")
    if(NOT sum_ok)
      gbl_finding(profile timing-arithmetic "dots_visible + dots_hblank overflows signed arithmetic range")
    elseif(NOT sum EQUAL T_dots_per_line)
      gbl_finding(profile timing-arithmetic "dots_visible + dots_hblank != dots_per_line")
    endif()
  endif()
  if(NOT "${T_dots_per_line}" STREQUAL "" AND NOT "${T_cycles_per_dot}" STREQUAL ""
      AND NOT "${T_cycles_per_line}" STREQUAL "")
    gbl__profile_safe_product(product_ok prod "${T_dots_per_line}" "${T_cycles_per_dot}")
    if(NOT product_ok)
      gbl_finding(profile timing-arithmetic "dots_per_line * cycles_per_dot overflows signed arithmetic range")
    elseif(NOT prod EQUAL T_cycles_per_line)
      gbl_finding(profile timing-arithmetic "dots_per_line * cycles_per_dot != cycles_per_line")
    endif()
  endif()
  if(NOT "${T_lines_visible}" STREQUAL "" AND NOT "${T_lines_vblank}" STREQUAL ""
      AND NOT "${T_lines_per_frame}" STREQUAL "")
    gbl__profile_safe_sum(sum_ok sum "${T_lines_visible}" "${T_lines_vblank}")
    if(NOT sum_ok)
      gbl_finding(profile timing-arithmetic "lines_visible + lines_vblank overflows signed arithmetic range")
    elseif(NOT sum EQUAL T_lines_per_frame)
      gbl_finding(profile timing-arithmetic "lines_visible + lines_vblank != lines_per_frame")
    endif()
  endif()
  if(NOT "${T_cycles_per_line}" STREQUAL "" AND NOT "${T_lines_per_frame}" STREQUAL ""
      AND NOT "${T_cycles_per_frame}" STREQUAL "")
    gbl__profile_safe_product(product_ok prod "${T_cycles_per_line}" "${T_lines_per_frame}")
    if(NOT product_ok)
      gbl_finding(profile timing-arithmetic "cycles_per_line * lines_per_frame overflows signed arithmetic range")
    elseif(NOT prod EQUAL T_cycles_per_frame)
      gbl_finding(profile timing-arithmetic "cycles_per_line * lines_per_frame != cycles_per_frame")
    endif()
  endif()
  if(NOT "${T_master_clock_hz}" STREQUAL "")
    math(EXPR clock "1 << 24")
    if(NOT clock EQUAL T_master_clock_hz)
      gbl_finding(profile timing-arithmetic "master_clock_hz != 2^24")
    endif()
  endif()
endfunction()

function(gbl__profile_ppu_policy doc)
  gbl_json_type(policy_type "${doc}" ppu_policy)
  if(NOT "${policy_type}" STREQUAL "OBJECT")
    gbl_finding(profile ppu-policy-missing
      "ppu_policy: missing or not an object")
    return()
  endif()

  string(JSON policy_version ERROR_VARIABLE version_error GET
    "${doc}" profile_version)
  if(NOT "${policy_version}" MATCHES "^[1-9][0-9]*$" OR
      policy_version LESS 15)
    gbl_finding(profile ppu-policy-value
      "profile_version: expected 15 or later for window/effect/mosaic and forced-blank policy metadata")
  endif()

  list(LENGTH GBL_PROFILE_PPU_POLICY_ROWS scalar_field_count)
  math(EXPR expected_field_count "${scalar_field_count}+8")
  string(JSON actual_field_count LENGTH "${doc}" ppu_policy)
  if(NOT actual_field_count EQUAL expected_field_count)
    gbl_finding(profile ppu-policy-value
      "ppu_policy: expected ${expected_field_count} fields, found ${actual_field_count}")
  endif()

  foreach(pair IN LISTS GBL_PROFILE_PPU_POLICY_ROWS)
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 key)
    list(GET fields 1 expected_value)
    list(GET fields 2 expected_type)
    set(path "ppu_policy.${key}")
    gbl_json_type(actual_type "${doc}" ppu_policy "${key}")
    if(NOT "${actual_type}" STREQUAL "${expected_type}")
      gbl_finding(profile ppu-policy-value
        "${path}: expected ${expected_type}, found ${actual_type}")
      continue()
    endif()
    string(JSON actual_value ERROR_VARIABLE value_error GET
      "${doc}" ppu_policy "${key}")
    if(NOT "${value_error}" STREQUAL "NOTFOUND" AND
        NOT "${value_error}" STREQUAL "")
      gbl_finding(profile ppu-policy-value "${path}: unreadable value")
    elseif(NOT "${actual_value}" STREQUAL "${expected_value}")
      gbl_finding(profile ppu-policy-value
        "${path}: expected ${expected_value}")
    endif()
  endforeach()

  gbl__profile_dispute("${doc}" "ppu_policy.hblank_dispute"
    ppu_policy hblank_dispute)
  gbl__profile_ppu_policy_object("${doc}" object_policy
    ppu-object-policy-value "${GBL_PROFILE_PPU_OBJECT_POLICY_JSON}"
    ${GBL_PROFILE_PPU_OBJECT_POLICY_FIELDS})
  gbl__profile_ppu_policy_object("${doc}" composition_policy
    ppu-composition-policy-value "${GBL_PROFILE_PPU_COMPOSITION_POLICY_JSON}"
    ${GBL_PROFILE_PPU_COMPOSITION_POLICY_FIELDS})
  gbl__profile_ppu_policy_object("${doc}" window_policy
    ppu-window-policy-value "${GBL_PROFILE_PPU_WINDOW_POLICY_JSON}"
    ${GBL_PROFILE_PPU_WINDOW_POLICY_FIELDS})
  gbl__profile_ppu_policy_object("${doc}" effect_policy
    ppu-effect-policy-value "${GBL_PROFILE_PPU_EFFECT_POLICY_JSON}"
    ${GBL_PROFILE_PPU_EFFECT_POLICY_FIELDS})
  gbl__profile_ppu_policy_object("${doc}" mosaic_policy
    ppu-mosaic-policy-value "${GBL_PROFILE_PPU_MOSAIC_POLICY_JSON}"
    ${GBL_PROFILE_PPU_MOSAIC_POLICY_FIELDS})
  gbl__profile_ppu_policy_object("${doc}" forced_blank_policy
    ppu-forced-blank-policy-value "${GBL_PROFILE_PPU_FORCED_BLANK_POLICY_JSON}"
    ${GBL_PROFILE_PPU_FORCED_BLANK_POLICY_FIELDS})
  gbl__profile_ppu_display_capabilities("${doc}")
  gbl__profile_ppu_video_memory_access("${doc}")
endfunction()

function(gbl__profile_audio_policy profile)
  gbl_json_type(policy_type "${profile}" audio_policy)
  if(NOT "${policy_type}" STREQUAL "OBJECT")
    gbl_finding(profile audio-policy-missing
      "audio_policy: missing or not an object")
    return()
  endif()

  gbl_json_type(version_type "${profile}" profile_version)
  if(NOT "${version_type}" STREQUAL "NUMBER")
    gbl_finding(profile audio-policy-value
      "profile_version: expected 16 or later for audio policy")
  else()
    string(JSON profile_version GET "${profile}" profile_version)
    if(NOT profile_version MATCHES "^[1-9][0-9]*$" OR
        profile_version LESS 16)
      gbl_finding(profile audio-policy-value
        "profile_version: expected 16 or later for audio policy")
    endif()
  endif()

  list(LENGTH GBL_PROFILE_AUDIO_POLICY_ROWS scalar_count)
  math(EXPR expected_field_count "${scalar_count}+2")
  string(JSON actual_field_count LENGTH "${profile}" audio_policy)
  if(NOT actual_field_count EQUAL expected_field_count)
    gbl_finding(profile audio-policy-value
      "audio_policy: expected ${expected_field_count} fields, found ${actual_field_count}")
  endif()

  foreach(pair IN LISTS GBL_PROFILE_AUDIO_POLICY_ROWS)
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 key)
    list(GET fields 1 expected_value)
    list(GET fields 2 expected_type)
    gbl_json_type(actual_type "${profile}" audio_policy "${key}")
    if(NOT "${actual_type}" STREQUAL "${expected_type}")
      gbl_finding(profile audio-policy-value
        "audio_policy.${key}: expected ${expected_type}, found ${actual_type}")
      continue()
    endif()
    string(JSON actual_value ERROR_VARIABLE value_error GET
      "${profile}" audio_policy "${key}")
    if(NOT "${value_error}" STREQUAL "NOTFOUND" AND
        NOT "${value_error}" STREQUAL "")
      gbl_finding(profile audio-policy-value
        "audio_policy.${key}: unreadable value")
    elseif(NOT "${actual_value}" STREQUAL "${expected_value}")
      gbl_finding(profile audio-policy-value
        "audio_policy.${key}: expected ${expected_value}")
    endif()
  endforeach()

  gbl__profile_audio_policy_object("${profile}" soundbias
    "${GBL_PROFILE_AUDIO_SOUNDBIAS_JSON}"
    ${GBL_PROFILE_AUDIO_SOUNDBIAS_FIELDS})

  gbl_json_type(registers_type "${profile}" audio_policy registers)
  if(NOT "${registers_type}" STREQUAL "ARRAY")
    gbl_finding(profile audio-policy-value
      "audio_policy.registers: expected ARRAY, found ${registers_type}")
  else()
    string(JSON actual_registers ERROR_VARIABLE registers_error GET
      "${profile}" audio_policy registers)
    if(NOT "${registers_error}" STREQUAL "NOTFOUND" AND
        NOT "${registers_error}" STREQUAL "")
      gbl_finding(profile audio-policy-value
        "audio_policy.registers: unreadable value")
    else()
      string(JSON registers_match EQUAL "${actual_registers}"
        "${GBL_PROFILE_AUDIO_REGISTERS_JSON}")
      if(NOT registers_match)
        gbl_finding(profile audio-policy-value
          "audio_policy.registers: exact register policy differs")
      endif()
    endif()
  endif()
endfunction()

function(gbl__profile_audio_policy_object profile object_key expected_json)
  set(path "audio_policy.${object_key}")
  gbl_json_type(actual_type "${profile}" audio_policy "${object_key}")
  if(NOT "${actual_type}" STREQUAL "OBJECT")
    gbl_finding(profile audio-policy-value
      "${path}: expected OBJECT, found ${actual_type}")
    return()
  endif()

  string(JSON expected_count LENGTH "${expected_json}")
  string(JSON actual_count LENGTH "${profile}" audio_policy "${object_key}")
  if(NOT actual_count EQUAL expected_count)
    gbl_finding(profile audio-policy-value
      "${path}: expected ${expected_count} fields, found ${actual_count}")
  endif()

  foreach(field IN LISTS ARGN)
    string(JSON expected_type TYPE "${expected_json}" "${field}")
    gbl_json_type(actual_field_type "${profile}" audio_policy
      "${object_key}" "${field}")
    if(NOT "${actual_field_type}" STREQUAL "${expected_type}")
      gbl_finding(profile audio-policy-value
        "${path}.${field}: expected ${expected_type}, found ${actual_field_type}")
      continue()
    endif()
    string(JSON expected_value GET "${expected_json}" "${field}")
    string(JSON actual_value ERROR_VARIABLE value_error GET
      "${profile}" audio_policy "${object_key}" "${field}")
    if(NOT "${value_error}" STREQUAL "NOTFOUND" AND
        NOT "${value_error}" STREQUAL "")
      gbl_finding(profile audio-policy-value
        "${path}.${field}: unreadable value")
    elseif(NOT "${actual_value}" STREQUAL "${expected_value}")
      gbl_finding(profile audio-policy-value
        "${path}.${field}: expected ${expected_value}")
    endif()
  endforeach()
endfunction()

# Each nested policy is an exact, typed contract. Compare individual fields to
# the canonical JSON so a bad value reports the causal path, while the count
# rejects missing and unexpected fields.
function(gbl__profile_ppu_policy_object doc object_key code expected_json)
  set(path "ppu_policy.${object_key}")
  gbl_json_type(actual_type "${doc}" ppu_policy "${object_key}")
  if(NOT "${actual_type}" STREQUAL "OBJECT")
    gbl_finding(profile ${code}
      "${path}: expected OBJECT, found ${actual_type}")
    return()
  endif()

  string(JSON expected_count LENGTH "${expected_json}")
  string(JSON actual_count LENGTH "${doc}" ppu_policy "${object_key}")
  if(NOT actual_count EQUAL expected_count)
    gbl_finding(profile ${code}
      "${path}: expected ${expected_count} fields, found ${actual_count}")
  endif()

  foreach(field IN LISTS ARGN)
    string(JSON expected_type TYPE "${expected_json}" "${field}")
    string(JSON expected_value GET "${expected_json}" "${field}")
    gbl_json_type(actual_field_type "${doc}" ppu_policy "${object_key}" "${field}")
    if(NOT "${actual_field_type}" STREQUAL "${expected_type}")
      gbl_finding(profile ${code}
        "${path}.${field}: expected ${expected_type}, found ${actual_field_type}")
      continue()
    endif()

    string(JSON actual_value ERROR_VARIABLE value_error GET
      "${doc}" ppu_policy "${object_key}" "${field}")
    if(NOT "${value_error}" STREQUAL "NOTFOUND" AND
        NOT "${value_error}" STREQUAL "")
      gbl_finding(profile ${code} "${path}.${field}: unreadable value")
    elseif(NOT "${actual_value}" STREQUAL "${expected_value}")
      gbl_finding(profile ${code}
        "${path}.${field}: expected ${expected_value}")
    endif()
  endforeach()
endfunction()

# The CPU/DMA × PRAM/VRAM/OAM × state product shares one selected availability
# policy. Bus request costs remain owned by bus_policy.request_cycles.
function(gbl__profile_ppu_video_memory_access doc)
  gbl_json_type(policy_type "${doc}" ppu_policy video_memory_access)
  if(NOT "${policy_type}" STREQUAL "OBJECT")
    gbl_finding(profile ppu-access-policy-value
      "ppu_policy.video_memory_access: expected OBJECT, found ${policy_type}")
    return()
  endif()
  string(JSON access_fields LENGTH "${doc}" ppu_policy video_memory_access)
  if(NOT access_fields EQUAL 8)
    gbl_finding(profile ppu-access-policy-value
      "ppu_policy.video_memory_access: expected 8 fields, found ${access_fields}")
  endif()
  string(JSON actual_access GET "${doc}" ppu_policy video_memory_access)
  string(JSON access_match EQUAL "${actual_access}" "${GBL_PROFILE_PPU_VIDEO_MEMORY_ACCESS_JSON}")
  if(NOT access_match)
    gbl_finding(profile ppu-access-policy-value
      "ppu_policy.video_memory_access: differs from selected CPU/DMA, PRAM/VRAM/OAM policy")
  endif()
endfunction()

function(gbl__profile_ppu_composition_features doc)
  set(path "ppu_policy.display_capabilities.composition_features")
  gbl_json_type(features_type "${doc}"
    ppu_policy display_capabilities composition_features)
  if(NOT "${features_type}" STREQUAL "OBJECT")
    gbl_finding(profile ppu-capability-value
      "${path}: expected OBJECT, found ${features_type}")
    return()
  endif()

  string(JSON expected_count LENGTH
    "${GBL_PROFILE_PPU_COMPOSITION_FEATURES_JSON}")
  string(JSON actual_count LENGTH "${doc}"
    ppu_policy display_capabilities composition_features)
  if(NOT actual_count EQUAL expected_count)
    gbl_finding(profile ppu-capability-value
      "${path}: expected ${expected_count} fields, found ${actual_count}")
  endif()

  foreach(field IN LISTS GBL_PROFILE_PPU_COMPOSITION_FEATURES_FIELDS)
    string(JSON expected_type TYPE
      "${GBL_PROFILE_PPU_COMPOSITION_FEATURES_JSON}" "${field}")
    string(JSON expected_value GET
      "${GBL_PROFILE_PPU_COMPOSITION_FEATURES_JSON}" "${field}")
    gbl_json_type(actual_type "${doc}"
      ppu_policy display_capabilities composition_features "${field}")
    if(NOT "${actual_type}" STREQUAL "${expected_type}")
      gbl_finding(profile ppu-capability-value
        "${path}.${field}: expected ${expected_type}, found ${actual_type}")
      continue()
    endif()
    string(JSON actual_value ERROR_VARIABLE value_error GET "${doc}"
      ppu_policy display_capabilities composition_features "${field}")
    if(NOT "${value_error}" STREQUAL "NOTFOUND" AND
        NOT "${value_error}" STREQUAL "")
      gbl_finding(profile ppu-capability-value "${path}.${field}: unreadable value")
    elseif(NOT "${actual_value}" STREQUAL "${expected_value}")
      gbl_finding(profile ppu-capability-value
        "${path}.${field}: expected ${expected_value}")
    endif()
  endforeach()
endfunction()

function(gbl__profile_ppu_display_capabilities doc)
  gbl_json_type(capability_type "${doc}" ppu_policy display_capabilities)
  if(NOT "${capability_type}" STREQUAL "OBJECT")
    gbl_finding(profile ppu-capability-value
      "ppu_policy.display_capabilities: expected OBJECT, found ${capability_type}")
    return()
  endif()
  string(JSON capability_fields LENGTH "${doc}" ppu_policy display_capabilities)
  if(NOT capability_fields EQUAL 4)
    gbl_finding(profile ppu-capability-value
      "ppu_policy.display_capabilities: expected 4 fields, found ${capability_fields}")
  endif()
  gbl_json_string(model "${doc}" ppu_policy display_capabilities model)
  if(NOT "${model}" STREQUAL "gamebloke-display-capabilities-2")
    gbl_finding(profile ppu-capability-value
      "ppu_policy.display_capabilities.model: expected gamebloke-display-capabilities-2")
  endif()
  gbl__profile_ppu_composition_features("${doc}")

  gbl_json_type(raw_type "${doc}" ppu_policy display_capabilities raw_output)
  if(NOT "${raw_type}" STREQUAL "OBJECT")
    gbl_finding(profile ppu-capability-value
      "ppu_policy.display_capabilities.raw_output: expected OBJECT, found ${raw_type}")
  else()
    string(JSON raw_fields LENGTH "${doc}" ppu_policy display_capabilities raw_output)
    if(NOT raw_fields EQUAL 5)
      gbl_finding(profile ppu-capability-value
        "ppu_policy.display_capabilities.raw_output: expected 5 fields, found ${raw_fields}")
    endif()
    string(JSON raw_output_json GET "${doc}" ppu_policy display_capabilities raw_output)
    string(JSON raw_output_match EQUAL "${raw_output_json}" "${GBL_PROFILE_PPU_RAW_OUTPUT_JSON}")
    if(NOT raw_output_match)
      gbl_finding(profile ppu-capability-value
        "ppu_policy.display_capabilities.raw_output: format or geometry differs from BGR555_LE 240x160 contract")
    endif()
  endif()

  gbl_json_type(modes_type "${doc}" ppu_policy display_capabilities modes)
  if(NOT "${modes_type}" STREQUAL "ARRAY")
    gbl_finding(profile ppu-capability-value
      "ppu_policy.display_capabilities.modes: expected ARRAY, found ${modes_type}")
  else()
    string(JSON mode_count LENGTH "${doc}" ppu_policy display_capabilities modes)
    if(NOT mode_count EQUAL 6)
      gbl_finding(profile ppu-capability-value
        "ppu_policy.display_capabilities.modes: expected 6 ordered records, found ${mode_count}")
    endif()
    string(JSON modes_json GET "${doc}" ppu_policy display_capabilities modes)
    string(JSON modes_match EQUAL "${modes_json}" "${GBL_PROFILE_PPU_EXPECTED_MODES_JSON}")
    if(NOT modes_match)
      gbl_finding(profile ppu-capability-value
        "ppu_policy.display_capabilities.modes: differs from the ordered mode/source/dimension/status contract")
    endif()
  endif()
endfunction()

# gbl__profile_unmodeled(<out-ids> <doc>): validate the unmodeled list and set
# <out-ids> to the entry ids in file order (duplicates kept).
function(gbl__profile_unmodeled out doc)
  set(ids "")
  gbl_json_length(count "${doc}" unmodeled)
  if(count LESS 1)
    set(${out} "" PARENT_SCOPE)
    return()
  endif()
  set(seen "")
  math(EXPR last "${count}-1")
  foreach(i RANGE ${last})
    gbl_json_type(ety "${doc}" unmodeled ${i})
    if(NOT "${ety}" STREQUAL "OBJECT")
      gbl_finding(profile unmodeled-bad-entry "unmodeled[${i}]: not an object")
      continue()
    endif()
    gbl_json_string(id "${doc}" unmodeled ${i} id)
    if("${id}" STREQUAL "<missing>")
      gbl_finding(profile unmodeled-bad-entry "unmodeled[${i}]: id")
      continue()
    endif()
    list(APPEND ids "${id}")
    if(NOT "${id}" MATCHES "^[a-z0-9][a-z0-9-]*$")
      gbl_finding(profile unmodeled-bad-entry "unmodeled[${i}]: bad id")
    endif()
    if("${id}" IN_LIST seen)
      gbl_finding(profile unmodeled-bad-entry "unmodeled[${i}]: duplicate id ${id}")
    endif()
    list(APPEND seen "${id}")

    gbl_json_string(status "${doc}" unmodeled ${i} status)
    if(NOT "${status}" STREQUAL "unmodeled")
      gbl_finding(profile unmodeled-bad-entry "${id}: status")
    endif()
    gbl_json_string(reason "${doc}" unmodeled ${i} reason)
    if("${reason}" STREQUAL "<missing>")
      gbl_finding(profile unmodeled-bad-entry "${id}: reason")
    endif()
    gbl__profile_dispute("${doc}" "unmodeled.${id}" unmodeled ${i})

    if("${id}" STREQUAL "sp-micro-differences")
      set(has_reg FALSE)
      set(has_sw FALSE)
      gbl_json_length(dlen "${doc}" unmodeled ${i} details)
      if(dlen GREATER 0)
        math(EXPR dlast "${dlen}-1")
        foreach(d RANGE ${dlast})
          gbl_json_string(item "${doc}" unmodeled ${i} details ${d})
          string(FIND "${item}" "0x04000800" p1)
          string(FIND "${item}" "cartridge switch" p2)
          if(NOT p1 EQUAL -1)
            set(has_reg TRUE)
          endif()
          if(NOT p2 EQUAL -1)
            set(has_sw TRUE)
          endif()
        endforeach()
      endif()
      if(NOT has_reg)
        gbl_finding(profile unmodeled-bad-entry "${id}: details lack the 0x04000800 item")
      endif()
      if(NOT has_sw)
        gbl_finding(profile unmodeled-bad-entry "${id}: details lack the cartridge switch item")
      endif()
    elseif("${id}" STREQUAL "hblank-flag-edge")
      gbl_json_string(did "${doc}" unmodeled ${i} dispute)
      if(NOT "${did}" STREQUAL "${GBL_PROFILE_HBLANK_DISPUTE}")
        gbl_finding(profile unmodeled-bad-entry "${id}: dispute")
      endif()
    endif()
  endforeach()

  foreach(req IN LISTS GBL_PROFILE_REQUIRED_UNMODELED)
    if(NOT "${req}" IN_LIST ids)
      gbl_finding(profile unmodeled-missing "${req}")
    endif()
  endforeach()
  set(${out} "${ids}" PARENT_SCOPE)
endfunction()

# Hide fenced and indented code before looking for policy tables. This keeps
# examples in a section from standing in for rendered Markdown content.
function(gbl__profile_doc_without_code out content)
  set(remaining "${content}")
  set(visible "")
  set(fence_char "")
  set(fence_width 0)
  while(TRUE)
    string(FIND "${remaining}" "\n" line_end)
    if(line_end EQUAL -1)
      set(line "${remaining}")
      set(remaining "")
      set(last_line TRUE)
    else()
      string(SUBSTRING "${remaining}" 0 ${line_end} line)
      math(EXPR next_line "${line_end}+1")
      string(SUBSTRING "${remaining}" ${next_line} -1 remaining)
      set(last_line FALSE)
    endif()

    string(LENGTH "${line}" line_length)
    set(indent 0)
    set(indent_length 0)
    while(indent_length LESS line_length)
      string(SUBSTRING "${line}" ${indent_length} 1 character)
      if("${character}" STREQUAL " ")
        math(EXPR indent "${indent}+1")
        math(EXPR indent_length "${indent_length}+1")
      elseif("${character}" STREQUAL "\t")
        math(EXPR indent "(${indent}/4+1)*4")
        math(EXPR indent_length "${indent_length}+1")
      else()
        break()
      endif()
    endwhile()

    set(code_line FALSE)
    if(NOT "${fence_char}" STREQUAL "")
      if(indent LESS 4)
        string(SUBSTRING "${line}" ${indent_length} -1 candidate)
        string(LENGTH "${candidate}" candidate_length)
        set(run_length 0)
        while(run_length LESS candidate_length)
          string(SUBSTRING "${candidate}" ${run_length} 1 character)
          if(NOT "${character}" STREQUAL "${fence_char}")
            break()
          endif()
          math(EXPR run_length "${run_length}+1")
        endwhile()
        if(run_length GREATER_EQUAL fence_width)
          string(SUBSTRING "${candidate}" ${run_length} -1 suffix)
          if("${suffix}" MATCHES "^[ \t]*$")
            set(fence_char "")
            set(fence_width 0)
          endif()
        endif()
      endif()
      set(code_line TRUE)
    elseif(indent GREATER_EQUAL 4 OR "${line}" MATCHES "^\t")
      set(code_line TRUE)
    elseif(indent_length LESS line_length)
      string(SUBSTRING "${line}" ${indent_length} -1 candidate)
      string(SUBSTRING "${candidate}" 0 1 first_character)
      if("${first_character}" STREQUAL "`" OR "${first_character}" STREQUAL "~")
        string(LENGTH "${candidate}" candidate_length)
        set(run_length 0)
        while(run_length LESS candidate_length)
          string(SUBSTRING "${candidate}" ${run_length} 1 character)
          if(NOT "${character}" STREQUAL "${first_character}")
            break()
          endif()
          math(EXPR run_length "${run_length}+1")
        endwhile()
        if(run_length GREATER_EQUAL 3)
          set(fence_char "${first_character}")
          set(fence_width ${run_length})
          set(code_line TRUE)
        endif()
      endif()
    endif()

    if(code_line)
      string(APPEND visible "\n")
    else()
      string(APPEND visible "${line}\n")
    endif()
    if(last_line)
      break()
    endif()
  endwhile()
  set(${out} "${visible}" PARENT_SCOPE)
endfunction()

# Remove HTML comments while leaving a blank-line boundary around each block.
# The boundary prevents Markdown rows on either side of a comment from joining
# into a synthetic contiguous table.
function(gbl__profile_doc_without_html_comments out content)
  set(remaining "${content}")
  set(visible "")
  while(TRUE)
    string(FIND "${remaining}" "<!--" comment_start)
    if(comment_start EQUAL -1)
      string(APPEND visible "${remaining}")
      break()
    endif()
    if(comment_start GREATER 0)
      string(SUBSTRING "${remaining}" 0 ${comment_start} prefix)
      string(APPEND visible "${prefix}")
    endif()

    math(EXPR comment_body_start "${comment_start}+4")
    string(SUBSTRING "${remaining}" ${comment_body_start} -1 comment_tail)
    string(FIND "${comment_tail}" "-->" comment_end)
    if(comment_end EQUAL -1)
      # An unterminated HTML comment consumes the rest of this document.
      string(APPEND visible "\n")
      break()
    endif()

    math(EXPR after_comment "${comment_end}+3")
    string(SUBSTRING "${comment_tail}" ${after_comment} -1 remaining)
    string(APPEND visible "\n")
  endwhile()
  set(${out} "${visible}" PARENT_SCOPE)
endfunction()

# Transform CommonMark inline code spans using matching backtick-run widths.
# This scans the whole document so a span may cross line breaks. MULTILINE
# preserves same-line spans (for example table identifiers) while masking any
# matched span containing a newline. ALL masks every matched span for the
# unsupported raw-HTML check. Newlines are preserved in both modes.
function(gbl__profile_doc_transform_inline_code out content mode)
  set(remaining "${content}")
  set(visible "")
  while(TRUE)
    string(FIND "${remaining}" "`" opening_start)
    if(opening_start EQUAL -1)
      string(APPEND visible "${remaining}")
      break()
    endif()
    if(opening_start GREATER 0)
      string(SUBSTRING "${remaining}" 0 ${opening_start} prefix)
      string(APPEND visible "${prefix}")
    endif()

    string(LENGTH "${remaining}" remaining_length)
    set(position ${opening_start})
    set(opening_width 0)
    while(position LESS remaining_length)
      string(SUBSTRING "${remaining}" ${position} 1 character)
      if(NOT "${character}" STREQUAL "`")
        break()
      endif()
      math(EXPR opening_width "${opening_width}+1")
      math(EXPR position "${position}+1")
    endwhile()

    set(search_position ${position})
    set(closing_start -1)
    while(search_position LESS remaining_length)
      string(SUBSTRING "${remaining}" ${search_position} -1 search_tail)
      string(FIND "${search_tail}" "`" next_tick)
      if(next_tick EQUAL -1)
        break()
      endif()
      math(EXPR candidate_start "${search_position}+${next_tick}")
      set(candidate_end ${candidate_start})
      set(candidate_width 0)
      while(candidate_end LESS remaining_length)
        string(SUBSTRING "${remaining}" ${candidate_end} 1 character)
        if(NOT "${character}" STREQUAL "`")
          break()
        endif()
        math(EXPR candidate_width "${candidate_width}+1")
        math(EXPR candidate_end "${candidate_end}+1")
      endwhile()
      if(candidate_width EQUAL opening_width)
        set(closing_start ${candidate_start})
        break()
      endif()
      set(search_position ${candidate_end})
    endwhile()

    if(closing_start EQUAL -1)
      string(REPEAT "`" ${opening_width} opening_ticks)
      string(APPEND visible "${opening_ticks}")
      string(SUBSTRING "${remaining}" ${position} -1 remaining)
      continue()
    endif()

    math(EXPR span_length "${closing_start}+${opening_width}-${opening_start}")
    string(SUBSTRING "${remaining}" ${opening_start} ${span_length} span)
    string(FIND "${span}" "\n" span_newline)
    if(mode STREQUAL "MULTILINE" AND span_newline EQUAL -1)
      string(APPEND visible "${span}")
    else()
      string(LENGTH "${span}" span_length)
      set(span_position 0)
      while(span_position LESS span_length)
        string(SUBSTRING "${span}" ${span_position} 1 character)
        if("${character}" STREQUAL "\n")
          string(APPEND visible "\n")
        else()
          string(APPEND visible " ")
        endif()
        math(EXPR span_position "${span_position}+1")
      endwhile()
    endif()
    math(EXPR after_span "${closing_start}+${opening_width}")
    string(SUBSTRING "${remaining}" ${after_span} -1 remaining)
  endwhile()
  set(${out} "${visible}" PARENT_SCOPE)
endfunction()

# Normalize the one canonical profile document before looking up any
# synchronized value. Its supported format is plain Markdown: fenced and
# indented examples and HTML comments are ignored; raw HTML blocks are rejected.
# Mask multiline inline spans while retaining same-line code identifiers.
function(gbl__profile_doc_normalize out content)
  gbl__profile_doc_without_code(code_free "${content}")
  gbl__profile_doc_transform_inline_code(multiline_code_free "${code_free}" MULTILINE)
  gbl__profile_doc_without_html_comments(comment_free "${multiline_code_free}")
  set(${out} "${comment_free}" PARENT_SCOPE)
endfunction()

# Remove inline code spans only for the raw-HTML check. Keep same-line spans in
# the normalized document because table cells use inline code for identifiers.
function(gbl__profile_doc_without_inline_code out content)
  gbl__profile_doc_transform_inline_code(code_free "${content}" ALL)
  set(${out} "${code_free}" PARENT_SCOPE)
endfunction()

# The canonical profile supports pipe tables, not raw HTML. Reject HTML
# elements and declaration/processing-instruction forms after Markdown
# examples, comments, and inline code are removed.
function(gbl__profile_doc_unsupported_html out content)
  gbl__profile_doc_without_inline_code(code_free "${content}")
  string(TOLOWER "${code_free}" lower_content)
  string(FIND "${lower_content}" "<!" declaration_at)
  string(FIND "${lower_content}" "<?" processing_instruction_at)
  if(NOT declaration_at EQUAL -1 OR NOT processing_instruction_at EQUAL -1)
    set(${out} "declaration or processing instruction" PARENT_SCOPE)
    return()
  endif()
  string(REGEX MATCH
    "<[/]?[ \t]*([a-z][a-z0-9-]*)([ \t\r\n/>])"
    matched_tag "${lower_content}")
  if(NOT "${matched_tag}" STREQUAL "")
    set(${out} "element <${CMAKE_MATCH_1}>" PARENT_SCOPE)
  else()
    set(${out} "" PARENT_SCOPE)
  endif()
endfunction()

# Split one outer-pipe row into trimmed cells. This validator deliberately
# supports the canonical pipe-table form rather than guessing other Markdown.
function(gbl__profile_doc_pipe_cells out line)
  if(NOT "${line}" MATCHES "^[ \t]*\\|.*\\|[ \t]*$")
    set(${out} "" PARENT_SCOPE)
    return()
  endif()
  string(REGEX REPLACE "^[ \t]*\\|" "" inner "${line}")
  string(REGEX REPLACE "\\|[ \t]*$" "" inner "${inner}")
  string(REPLACE "|" ";" cells "${inner}")
  set(trimmed_cells "")
  foreach(cell IN LISTS cells)
    string(STRIP "${cell}" cell)
    list(APPEND trimmed_cells "${cell}")
  endforeach()
  set(${out} "${trimmed_cells}" PARENT_SCOPE)
endfunction()

# Count separators and read the first cell without converting the whole row to
# a CMake list; citation semicolons are ordinary table content.
function(gbl__profile_doc_pipe_row_shape out_count out_first_cell line)
  if(NOT "${line}" MATCHES "^[ \t]*\\|.*\\|[ \t]*$")
    set(${out_count} 0 PARENT_SCOPE)
    set(${out_first_cell} "" PARENT_SCOPE)
    return()
  endif()
  string(REGEX REPLACE "^[ \t]*\\|" "" inner "${line}")
  string(REGEX REPLACE "\\|[ \t]*$" "" inner "${inner}")
  set(remaining "${inner}")
  set(cell_count 1)
  set(first_separator -1)
  while(TRUE)
    string(FIND "${remaining}" "|" separator_at)
    if(separator_at EQUAL -1)
      break()
    endif()
    if(first_separator EQUAL -1)
      set(first_separator ${separator_at})
    endif()
    math(EXPR cell_count "${cell_count}+1")
    math(EXPR after_separator "${separator_at}+1")
    string(SUBSTRING "${remaining}" ${after_separator} -1 remaining)
  endwhile()
  if(first_separator EQUAL -1)
    set(first_cell "${inner}")
  else()
    string(SUBSTRING "${inner}" 0 ${first_separator} first_cell)
  endif()
  string(STRIP "${first_cell}" first_cell)
  set(${out_count} ${cell_count} PARENT_SCOPE)
  set(${out_first_cell} "${first_cell}" PARENT_SCOPE)
endfunction()

# A canonical pipe table has the exact documented header and a delimiter cell
# with at least one hyphen for every header column.
function(gbl__profile_doc_table_structure_valid out header_line separator_line expected_header)
  gbl__profile_doc_pipe_cells(header_cells "${header_line}")
  gbl__profile_doc_pipe_cells(separator_cells "${separator_line}")
  list(LENGTH expected_header expected_count)
  list(LENGTH header_cells header_count)
  list(LENGTH separator_cells separator_count)
  if(NOT header_count EQUAL expected_count OR NOT separator_count EQUAL expected_count)
    set(${out} FALSE PARENT_SCOPE)
    return()
  endif()

  set(index 0)
  foreach(expected_cell IN LISTS expected_header)
    list(GET header_cells ${index} actual_cell)
    list(GET separator_cells ${index} delimiter_cell)
    if(NOT "${actual_cell}" STREQUAL "${expected_cell}" OR
       NOT "${delimiter_cell}" MATCHES "^:?-+:?$")
      set(${out} FALSE PARENT_SCOPE)
      return()
    endif()
    math(EXPR index "${index}+1")
  endforeach()
  set(${out} TRUE PARENT_SCOPE)
endfunction()

# A supported policy table is a contiguous pipe table with the exact header,
# a matching valid separator, and body rows. Only this table is checked.
function(gbl__profile_doc_first_table out content prefix expected_header)
  set(remaining "${content}")
  set(previous_line "")
  set(previous_is_row FALSE)
  set(in_table FALSE)
  set(table "")
  while(TRUE)
    string(FIND "${remaining}" "\n" line_end)
    if(line_end EQUAL -1)
      set(line "${remaining}")
      set(remaining "")
      set(last_line TRUE)
    else()
      string(SUBSTRING "${remaining}" 0 ${line_end} line)
      math(EXPR next_line "${line_end}+1")
      string(SUBSTRING "${remaining}" ${next_line} -1 remaining)
      set(last_line FALSE)
    endif()

    if(in_table)
      if("${line}" MATCHES "^[ \t]*\\|.*\\|[ \t]*$")
        string(APPEND table "${line}\n")
      else()
        # A nonblank continuation is not a supported end-of-table marker.
        # In particular, GFM rows without outer pipes must not be ignored.
        string(STRIP "${line}" boundary)
        if(NOT "${boundary}" STREQUAL "" AND NOT "${boundary}" MATCHES "^(#|##|###|####|#####|######)([ \t]|$)")
          set(${out} "" PARENT_SCOPE)
          return()
        endif()
        string(FIND "${table}" "\n| ${prefix}." policy_row_at)
        if(policy_row_at EQUAL -1)
          string(FIND "${table}" "\n| `${prefix}." policy_row_at)
        endif()
        if(NOT policy_row_at EQUAL -1)
          break()
        endif()
        set(table "")
        set(in_table FALSE)
        set(previous_is_row FALSE)
      endif()
    elseif(previous_is_row AND "${line}" MATCHES "^[ \t]*\\|.*\\|[ \t]*$")
      gbl__profile_doc_table_structure_valid(valid_table "${previous_line}" "${line}" "${expected_header}")
      if(valid_table)
        set(table "${previous_line}\n${line}\n")
        set(in_table TRUE)
      else()
        set(previous_line "${line}")
        set(previous_is_row TRUE)
      endif()
    else()
      if("${line}" MATCHES "^[ \t]*\\|.*\\|[ \t]*$")
        set(previous_line "${line}")
        set(previous_is_row TRUE)
      else()
        set(previous_is_row FALSE)
      endif()
    endif()
    if(last_line)
      break()
    endif()
  endwhile()
  if(in_table)
    string(FIND "${table}" "\n| ${prefix}." policy_row_at)
    if(policy_row_at EQUAL -1)
      string(FIND "${table}" "\n| `${prefix}." policy_row_at)
    endif()
    if(policy_row_at EQUAL -1)
      set(table "")
    endif()
  endif()
  set(${out} "${table}" PARENT_SCOPE)
endfunction()

# Find the supported unmodeled table by its exact header and valid separator,
# then return only its contiguous pipe rows with two cells and a full ID cell.
# Other pipe text cannot supply IDs.
function(gbl__profile_doc_unmodeled_table out content)
  set(remaining "${content}")
  set(header_line "")
  set(expect_separator FALSE)
  set(in_table FALSE)
  set(table "")
  while(TRUE)
    string(FIND "${remaining}" "\n" line_end)
    if(line_end EQUAL -1)
      set(line "${remaining}")
      set(remaining "")
      set(last_line TRUE)
    else()
      string(SUBSTRING "${remaining}" 0 ${line_end} line)
      math(EXPR next_line "${line_end}+1")
      string(SUBSTRING "${remaining}" ${next_line} -1 remaining)
      set(last_line FALSE)
    endif()

    if(in_table)
      if("${line}" MATCHES "^[ \t]*\\|.*\\|[ \t]*$")
        string(APPEND table "${line}\n")
      else()
        string(STRIP "${line}" boundary)
        if(NOT "${boundary}" STREQUAL "" AND NOT "${boundary}" MATCHES "^(#|##|###|####|#####|######)([ \t]|$)")
          set(${out} "" PARENT_SCOPE)
          return()
        endif()
        break()
      endif()
    elseif(expect_separator)
      if("${line}" MATCHES "^[ \t]*\\|.*\\|[ \t]*$")
        gbl__profile_doc_table_structure_valid(valid_table "${header_line}" "${line}"
          "Id;What is not modeled")
        if(valid_table)
          set(table "${header_line}\n${line}\n")
          set(in_table TRUE)
        endif()
      endif()
      set(expect_separator FALSE)
    elseif("${line}" MATCHES "^[ \t]*\\|.*\\|[ \t]*$")
      set(header_line "${line}")
      set(expect_separator TRUE)
    endif()

    if(last_line)
      break()
    endif()
  endwhile()

  if(NOT in_table)
    set(table "")
    set(${out} "" PARENT_SCOPE)
    return()
  endif()

  set(remaining "${table}")
  set(line_index 0)
  set(body_row_count 0)
  set(table_valid TRUE)
  while(TRUE)
    string(FIND "${remaining}" "\n" line_end)
    if(line_end EQUAL -1)
      set(line "${remaining}")
      set(remaining "")
      set(last_line TRUE)
    else()
      string(SUBSTRING "${remaining}" 0 ${line_end} line)
      math(EXPR next_line "${line_end}+1")
      string(SUBSTRING "${remaining}" ${next_line} -1 remaining)
      set(last_line FALSE)
    endif()

    if(line_index GREATER 1 AND NOT "${line}" STREQUAL "")
      gbl__profile_doc_pipe_row_shape(cell_count id_cell "${line}")
      if(NOT cell_count EQUAL 2)
        set(table_valid FALSE)
        break()
      endif()
      if(NOT "${id_cell}" MATCHES "^`([a-z0-9][a-z0-9-]*)`$")
        set(table_valid FALSE)
        break()
      endif()
      math(EXPR body_row_count "${body_row_count}+1")
    endif()

    math(EXPR line_index "${line_index}+1")
    if(last_line)
      break()
    endif()
  endwhile()
  if(NOT table_valid OR body_row_count EQUAL 0)
    set(table "")
  endif()
  set(${out} "${table}" PARENT_SCOPE)
endfunction()

# Check one table in the already-normalized canonical document against rows
# built from the profile values. Matching rows elsewhere cannot satisfy it.
function(gbl__profile_doc_policy_table normalized_text heading prefix expected_count expected_rows_var)
  set(finding_code bus-policy-doc)
  set(expected_header "Setting;Value;Label;Citation")
  set(expected_cell_count 4)
  if(ARGC GREATER 5)
    set(expected_header "${ARGV5}")
  endif()
  if(ARGC GREATER 6)
    set(finding_code "${ARGV6}")
  endif()
  if(ARGC GREATER 7)
    set(expected_cell_count "${ARGV7}")
  endif()
  string(FIND "${normalized_text}" "\n${heading}\n" heading_at)
  if(heading_at EQUAL -1)
    gbl_finding(profile "${finding_code}" "missing ${heading} section")
    return()
  endif()
  string(LENGTH "\n${heading}\n" heading_length)
  math(EXPR section_start "${heading_at}+${heading_length}")
  string(SUBSTRING "${normalized_text}" ${section_start} -1 section)
  string(REGEX MATCH "\n#+[ \t]" next_heading "${section}")
  if(NOT "${next_heading}" STREQUAL "")
    string(FIND "${section}" "${next_heading}" next_heading_at)
    string(SUBSTRING "${section}" 0 ${next_heading_at} section)
  endif()

  gbl__profile_doc_first_table(table "${section}" "${prefix}"
    "${expected_header}")
  if("${table}" STREQUAL "")
    gbl_finding(profile "${finding_code}" "${prefix}: missing policy table")
    return()
  endif()

  set(expected_keys)
  foreach(row IN LISTS ${expected_rows_var})
    string(FIND "${row}" " |" key_end)
    math(EXPR key_length "${key_end}-2")
    string(SUBSTRING "${row}" 2 ${key_length} key)
    list(APPEND expected_keys "${key}")
  endforeach()

  set(remaining_table "${table}")
  set(line_index 0)
  set(body_row_count 0)
  while(TRUE)
    string(FIND "${remaining_table}" "\n" line_end)
    if(line_end EQUAL -1)
      set(line "${remaining_table}")
      set(remaining_table "")
      set(last_line TRUE)
    else()
      string(SUBSTRING "${remaining_table}" 0 ${line_end} line)
      math(EXPR next_line "${line_end}+1")
      string(SUBSTRING "${remaining_table}" ${next_line} -1 remaining_table)
      set(last_line FALSE)
    endif()

    if(line_index GREATER 1 AND NOT "${line}" STREQUAL "")
      gbl__profile_doc_pipe_row_shape(cell_count row_key "${line}")
      if(NOT cell_count EQUAL expected_cell_count)
        gbl_finding(profile "${finding_code}"
          "${prefix}: expected ${expected_cell_count} cells per body row, found ${cell_count}")
      else()
        if(NOT "${row_key}" IN_LIST expected_keys)
          gbl_finding(profile "${finding_code}" "${prefix}: unexpected policy row ${row_key}")
        endif()
      endif()
      math(EXPR body_row_count "${body_row_count}+1")
    endif()

    math(EXPR line_index "${line_index}+1")
    if(last_line)
      break()
    endif()
  endwhile()
  if(NOT body_row_count EQUAL expected_count)
    gbl_finding(profile "${finding_code}" "${prefix}: expected ${expected_count} rows, found ${body_row_count}")
  endif()

  foreach(row IN LISTS ${expected_rows_var})
    string(FIND "${row}" " |" key_end)
    math(EXPR key_start "2")
    math(EXPR key_length "${key_end}-${key_start}")
    string(SUBSTRING "${row}" ${key_start} ${key_length} key)
    set(key_marker "\n| ${key} |")
    string(LENGTH "${key_marker}" key_marker_length)
    set(remaining_table "${table}")
    set(key_count 0)
    while(TRUE)
      string(FIND "${remaining_table}" "${key_marker}" key_at)
      if(key_at EQUAL -1)
        break()
      endif()
      math(EXPR key_count "${key_count}+1")
      math(EXPR after_key "${key_at}+${key_marker_length}")
      string(SUBSTRING "${remaining_table}" ${after_key} -1 remaining_table)
    endwhile()
    if(NOT key_count EQUAL 1)
      gbl_finding(profile "${finding_code}" "${prefix}: expected key ${key} exactly once in policy table, found ${key_count}")
    else()
      string(FIND "${table}" "\n${row}\n" at)
      if(at EQUAL -1)
        gbl_finding(profile "${finding_code}" "${prefix}: missing or stale row")
      endif()
    endif()
  endforeach()
endfunction()

# Compare a small policy table as a string so semicolons in canonical values
# remain part of a cell instead of becoming CMake list separators.
function(gbl__profile_doc_exact_policy_table normalized_text heading prefix
    expected_table finding_code)
  string(FIND "${normalized_text}" "\n${heading}\n" heading_at)
  if(heading_at EQUAL -1)
    gbl_finding(profile "${finding_code}" "missing ${heading} section")
    return()
  endif()
  string(LENGTH "\n${heading}\n" heading_length)
  math(EXPR section_start "${heading_at}+${heading_length}")
  string(SUBSTRING "${normalized_text}" ${section_start} -1 section)
  string(REGEX MATCH "\n#+[ \t]" next_heading "${section}")
  if(NOT "${next_heading}" STREQUAL "")
    string(FIND "${section}" "${next_heading}" next_heading_at)
    string(SUBSTRING "${section}" 0 ${next_heading_at} section)
  endif()
  gbl__profile_doc_first_table(table "${section}" "${prefix}"
    "Setting;Value")
  if("${table}" STREQUAL "")
    gbl_finding(profile "${finding_code}" "${prefix}: missing policy table")
    return()
  endif()
  string(STRIP "${table}" actual_table)
  string(STRIP "${expected_table}" expected_table)
  if(NOT "${actual_table}" STREQUAL "${expected_table}")
    gbl_finding(profile "${finding_code}" "${prefix}: missing or stale row")
  endif()
endfunction()

function(gbl__profile_doc_ppu_object_table out profile object_key)
  set(table "| Setting | Value |\n| --- | --- |\n")
  foreach(field IN LISTS ARGN)
    gbl_json_type(value_type "${profile}" ppu_policy "${object_key}" "${field}")
    if("${value_type}" STREQUAL "MISSING")
      set(value "<missing>")
    else()
      string(JSON value ERROR_VARIABLE value_error GET
        "${profile}" ppu_policy "${object_key}" "${field}")
      if(NOT "${value_error}" STREQUAL "NOTFOUND" AND
          NOT "${value_error}" STREQUAL "")
        set(value "<invalid>")
      elseif("${value_type}" STREQUAL "BOOLEAN")
        if(value)
          set(value true)
        else()
          set(value false)
        endif()
      endif()
    endif()
    string(APPEND table
      "| `ppu_policy.${object_key}.${field}` | ${value} |\n")
  endforeach()
  set(${out} "${table}" PARENT_SCOPE)
endfunction()

function(gbl__profile_doc_timing_sync normalized_text profile)
  foreach(heading IN ITEMS "## Timing model scope" "## Timing model costs"
      "## WAITCNT policy" "## DMA policy"
      "## CPU cycle composition" "## Timer edge policy"
      "## Timer 0 guest-path coverage" "## Unmodeled hardware cost gaps")
    string(FIND "${normalized_text}" "\n${heading}\n" heading_at)
    if(heading_at EQUAL -1)
      gbl_finding(profile timing-model-doc "missing ${heading} section")
    endif()
  endforeach()
  gbl_json_string(model_id "${profile}" timing_model id)
  gbl_json_string(qualification "${profile}" timing_model qualification)
  string(FIND "${normalized_text}" "\nModel ID: ${model_id}\n" model_line)
  string(FIND "${normalized_text}" "\nQualification: ${qualification}\n" qualification_line)
  if(model_line EQUAL -1 OR qualification_line EQUAL -1)
    gbl_finding(profile timing-model-doc "timing_model id or qualification is stale")
  endif()

  gbl_json_length(bus_count "${profile}" timing_model bus_access_costs)
  set(bus_rows)
  if(bus_count GREATER 0)
    math(EXPR last_bus "${bus_count}-1")
    foreach(i RANGE ${last_bus})
      gbl_json_string(key "${profile}" timing_model bus_access_costs ${i} key)
      gbl_json_string(region "${profile}" timing_model bus_access_costs ${i} region)
      gbl_json_string(exec_class "${profile}" timing_model bus_access_costs ${i} execution classification)
      gbl_json_type(exec_cost_type "${profile}" timing_model bus_access_costs ${i} execution cost_cycles)
      if("${exec_cost_type}" STREQUAL "NULL")
        gbl_json_string(exec_formula "${profile}" timing_model bus_access_costs ${i} execution formula)
        set(exec_cycles "formula: ${exec_formula}")
      else()
        string(JSON exec_cycles GET "${profile}" timing_model bus_access_costs ${i} execution cost_cycles)
      endif()
      gbl_json_string(exec_cite "${profile}" timing_model bus_access_costs ${i} execution citation)
      gbl_json_string(hw_class "${profile}" timing_model bus_access_costs ${i} hardware_cost classification)
      gbl_json_string(hw_cite "${profile}" timing_model bus_access_costs ${i} hardware_cost citation)
      gbl_json_length(gap_count "${profile}" timing_model bus_access_costs ${i} hardware_cost unmodeled_ids)
      set(gaps)
      if(gap_count GREATER 0)
        math(EXPR last_gap "${gap_count}-1")
        foreach(g RANGE ${last_gap})
          gbl_json_string(gap "${profile}" timing_model bus_access_costs ${i} hardware_cost unmodeled_ids ${g})
          list(APPEND gaps "${gap}")
        endforeach()
      endif()
      string(JOIN ", " gaps_text ${gaps})
      string(REPLACE ";" "\\;" exec_cite_row "${exec_cite}")
      string(REPLACE ";" "\\;" hw_cite_row "${hw_cite}")
      list(APPEND bus_rows
        "| `timing_model.${key}` | ${exec_class} | ${exec_cycles} | ${exec_cite_row} | ${hw_class} | null | ${hw_cite_row} | ${gaps_text} |")
    endforeach()
  endif()
  gbl__profile_doc_policy_table("${normalized_text}" "## Timing model costs"
    timing_model ${bus_count} bus_rows
    "Key;Execution class;Execution cycles / formula;Execution citation;Hardware-cost class;Hardware cycles;Hardware citation;Unmodeled limits"
    timing-model-doc 8)

  foreach(section_key IN ITEMS
      "waitcnt_policy.register" "waitcnt_policy.window.ws0" "waitcnt_policy.window.ws1"
      "waitcnt_policy.window.ws2" "waitcnt_policy.beat_policy" "waitcnt_policy.boundary"
      "waitcnt_policy.prefetch" "waitcnt_policy.hardware_qualification"
      "dma_policy.registers" "dma_policy.source_facts" "dma_policy.trigger_policy"
      "dma_policy.completion_policy" "dma_policy.startup_policy"
      "dma_policy.arbitration_policy" "dma_policy.latch_policy"
      "dma_policy.hardware_qualification")
    string(FIND "${normalized_text}" "| ${section_key} |" section_row)
    if(section_row EQUAL -1)
      gbl_finding(profile timing-model-doc "missing policy table row ${section_key}")
    endif()
  endforeach()
  string(FIND "${normalized_text}" "| waitcnt_policy.hardware_qualification | UNKNOWN | unmodeled |" waitcnt_qualification)
  string(FIND "${normalized_text}" "| dma_policy.hardware_qualification | UNKNOWN | unmodeled |" dma_qualification)
  if(waitcnt_qualification EQUAL -1 OR dma_qualification EQUAL -1)
    gbl_finding(profile timing-model-doc "WAITCNT and DMA hardware qualification must remain UNKNOWN in the document")
  endif()

  gbl_json_string(trigger_deferred "${profile}" dma_policy trigger_policy deferred_behavior)
  gbl_json_string(trigger_cite "${profile}" dma_policy trigger_policy cite)
  set(expected_trigger_row
    "| dma_policy.trigger_policy | enable bit 15; immediate mode 0 starts on rising enable edge and is one-shot; delayed modes [1,2,3] remain armed; ${trigger_deferred}; channel 0 mode 3 is refused | project-selected | ${trigger_cite} |")
  string(FIND "${normalized_text}" "\n${expected_trigger_row}\n" trigger_row_at)
  if(trigger_row_at EQUAL -1)
    gbl_finding(profile dma-policy-doc
      "dma_policy.trigger_policy: linked producer declaration or citation row is stale")
  endif()

  gbl_json_length(cpu_count "${profile}" timing_model cpu_composition)
  set(cpu_rows)
  if(cpu_count GREATER 0)
    math(EXPR last_cpu "${cpu_count}-1")
    foreach(i RANGE ${last_cpu})
      gbl_json_string(id "${profile}" timing_model cpu_composition ${i} id)
      gbl_json_string(classification "${profile}" timing_model cpu_composition ${i} classification)
      gbl_json_string(rule "${profile}" timing_model cpu_composition ${i} rule)
      gbl_json_string(citation "${profile}" timing_model cpu_composition ${i} citation)
      string(REPLACE ";" "\\;" rule_row "${rule}")
      string(REPLACE ";" "\\;" citation_row "${citation}")
      list(APPEND cpu_rows "| `timing_model.${id}` | ${classification} | ${rule_row} | ${citation_row} |")
    endforeach()
  endif()
  gbl__profile_doc_policy_table("${normalized_text}" "## CPU cycle composition"
    timing_model ${cpu_count} cpu_rows "Key;Classification;Rule;Citation" timing-model-doc 4)

  gbl_json_length(edge_count "${profile}" timing_model timer_edge_policies)
  set(edge_rows)
  if(edge_count GREATER 0)
    math(EXPR last_edge "${edge_count}-1")
    foreach(i RANGE ${last_edge})
      gbl_json_string(id "${profile}" timing_model timer_edge_policies ${i} id)
      gbl_json_string(classification "${profile}" timing_model timer_edge_policies ${i} classification)
      gbl_json_string(rule "${profile}" timing_model timer_edge_policies ${i} selected_rule)
      gbl_json_string(status "${profile}" timing_model timer_edge_policies ${i} hardware_qualification)
      gbl_json_string(citation "${profile}" timing_model timer_edge_policies ${i} citation)
      string(REPLACE ";" "\\;" rule_row "${rule}")
      string(REPLACE ";" "\\;" citation_row "${citation}")
      list(APPEND edge_rows "| `timing_model.${id}` | ${classification} | ${rule_row} | ${status} | ${citation_row} |")
    endforeach()
  endif()
  gbl__profile_doc_policy_table("${normalized_text}" "## Timer edge policy"
    timing_model ${edge_count} edge_rows
    "Key;Classification;Selected rule;Hardware qualification;Citation" timing-model-doc 5)

  gbl_json_length(guest_count "${profile}" timing_model guest_path_coverage)
  set(guest_rows)
  if(guest_count GREATER 0)
    math(EXPR last_guest "${guest_count}-1")
    foreach(i RANGE ${last_guest})
      gbl_json_string(id "${profile}" timing_model guest_path_coverage ${i} id)
      gbl_json_string(description "${profile}" timing_model guest_path_coverage ${i} description)
      gbl_json_string(key "${profile}" timing_model guest_path_coverage ${i} key)
      string(REPLACE ";" "\\;" description_row "${description}")
      list(APPEND guest_rows "| `timing_model.${id}` | ${description_row} | `${key}` |")
    endforeach()
  endif()
  gbl__profile_doc_policy_table("${normalized_text}" "## Timer 0 guest-path coverage"
    timing_model ${guest_count} guest_rows "Key;Guest access shape;Resolved cost key" timing-model-doc 3)

  gbl_json_length(gap_count "${profile}" timing_model unmodeled_hardware)
  set(hardware_gap_rows)
  if(gap_count GREATER 0)
    math(EXPR last_gap "${gap_count}-1")
    foreach(i RANGE ${last_gap})
      gbl_json_string(id "${profile}" timing_model unmodeled_hardware ${i} id)
      gbl_json_string(classification "${profile}" timing_model unmodeled_hardware ${i} classification)
      gbl_json_string(status "${profile}" timing_model unmodeled_hardware ${i} hardware_qualification)
      gbl_json_string(shapes "${profile}" timing_model unmodeled_hardware ${i} affected_shapes)
      gbl_json_string(citation "${profile}" timing_model unmodeled_hardware ${i} citation)
      string(REPLACE ";" "\\;" shapes_row "${shapes}")
      string(REPLACE ";" "\\;" citation_row "${citation}")
      list(APPEND hardware_gap_rows "| `timing_model.${id}` | ${classification} | ${status} | ${shapes_row} | ${citation_row} |")
    endforeach()
  endif()
  gbl__profile_doc_policy_table("${normalized_text}" "## Unmodeled hardware cost gaps"
    timing_model ${gap_count} hardware_gap_rows
    "Key;Classification;Hardware qualification;Affected shapes;Citation" timing-model-doc 5)
endfunction()

# Build the policy table from the profile's ordered PPU fields. The semantic
# checker owns the exact field set and values; this binds the canonical prose
# table to those same values without allowing a matching row elsewhere.
function(gbl__profile_doc_ppu_sync normalized_text profile)
  gbl_json_type(policy_type "${profile}" ppu_policy)
  if(NOT "${policy_type}" STREQUAL "OBJECT")
    return()
  endif()

  set(ppu_rows)
  foreach(pair IN LISTS GBL_PROFILE_PPU_POLICY_ROWS)
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 key)
    gbl_json_type(value_type "${profile}" ppu_policy "${key}")
    if("${value_type}" STREQUAL "MISSING")
      set(value "<missing>")
    else()
      string(JSON value ERROR_VARIABLE value_error GET
        "${profile}" ppu_policy "${key}")
      if(NOT "${value_error}" STREQUAL "NOTFOUND" AND
          NOT "${value_error}" STREQUAL "")
        set(value "<invalid>")
      endif()
    endif()
    string(REPLACE ";" "\\;" value_row "${value}")
    list(APPEND ppu_rows "| `ppu_policy.${key}` | ${value_row} |")
  endforeach()
  list(LENGTH ppu_rows ppu_count)
  gbl__profile_doc_policy_table("${normalized_text}"
    "## PPU behavior policy" ppu_policy ${ppu_count} ppu_rows
    "Setting;Value" ppu-policy-doc 2)

  set(object_policy_table "| Setting | Value |\n| --- | --- |\n")
  foreach(key IN LISTS GBL_PROFILE_PPU_OBJECT_POLICY_FIELDS)
    gbl_json_type(value_type "${profile}" ppu_policy object_policy "${key}")
    if("${value_type}" STREQUAL "MISSING")
      set(value "<missing>")
    else()
      string(JSON value ERROR_VARIABLE value_error GET
        "${profile}" ppu_policy object_policy "${key}")
      if(NOT "${value_error}" STREQUAL "NOTFOUND" AND
          NOT "${value_error}" STREQUAL "")
        set(value "<invalid>")
      elseif("${value_type}" STREQUAL "BOOLEAN")
        if(value)
          set(value true)
        else()
          set(value false)
        endif()
      endif()
    endif()
    string(APPEND object_policy_table
      "| `ppu_policy.object_policy.${key}` | ${value} |\n")
  endforeach()
  gbl__profile_doc_exact_policy_table("${normalized_text}"
    "## PPU object policy" ppu_policy.object_policy
    "${object_policy_table}" ppu-object-policy-doc)

  set(composition_policy_table "| Setting | Value |\n| --- | --- |\n")
  foreach(key IN LISTS GBL_PROFILE_PPU_COMPOSITION_POLICY_FIELDS)
    gbl_json_type(value_type "${profile}" ppu_policy composition_policy "${key}")
    if("${value_type}" STREQUAL "MISSING")
      set(value "<missing>")
    else()
      string(JSON value ERROR_VARIABLE value_error GET
        "${profile}" ppu_policy composition_policy "${key}")
      if(NOT "${value_error}" STREQUAL "NOTFOUND" AND
          NOT "${value_error}" STREQUAL "")
        set(value "<invalid>")
      endif()
    endif()
    string(APPEND composition_policy_table
      "| `ppu_policy.composition_policy.${key}` | ${value} |\n")
  endforeach()
  gbl__profile_doc_exact_policy_table("${normalized_text}"
    "## PPU composition policy" ppu_policy.composition_policy
    "${composition_policy_table}" ppu-composition-policy-doc)

  gbl__profile_doc_ppu_object_table(window_policy_table "${profile}"
    window_policy ${GBL_PROFILE_PPU_WINDOW_POLICY_FIELDS})
  gbl__profile_doc_exact_policy_table("${normalized_text}"
    "## PPU window policy" ppu_policy.window_policy
    "${window_policy_table}" ppu-window-policy-doc)

  gbl__profile_doc_ppu_object_table(effect_policy_table "${profile}"
    effect_policy ${GBL_PROFILE_PPU_EFFECT_POLICY_FIELDS})
  gbl__profile_doc_exact_policy_table("${normalized_text}"
    "## PPU color-effects policy" ppu_policy.effect_policy
    "${effect_policy_table}" ppu-effect-policy-doc)

  gbl__profile_doc_ppu_object_table(mosaic_policy_table "${profile}"
    mosaic_policy ${GBL_PROFILE_PPU_MOSAIC_POLICY_FIELDS})
  gbl__profile_doc_exact_policy_table("${normalized_text}"
    "## PPU mosaic policy" ppu_policy.mosaic_policy
    "${mosaic_policy_table}" ppu-mosaic-policy-doc)

  gbl__profile_doc_ppu_object_table(forced_blank_policy_table "${profile}"
    forced_blank_policy ${GBL_PROFILE_PPU_FORCED_BLANK_POLICY_FIELDS})
  gbl__profile_doc_exact_policy_table("${normalized_text}"
    "## Forced blank policy" ppu_policy.forced_blank_policy
    "${forced_blank_policy_table}" ppu-forced-blank-policy-doc)

  set(composition_features_table
    "| Setting | Value |\n| --- | --- |\n"
    "| `ppu_policy.display_capabilities.model` | gamebloke-display-capabilities-2 |\n"
    "| `ppu_policy.display_capabilities.composition_features.rectangular_windows` | true |\n"
    "| `ppu_policy.display_capabilities.composition_features.object_windows` | true |\n"
    "| `ppu_policy.display_capabilities.composition_features.alpha_blending` | true |\n"
    "| `ppu_policy.display_capabilities.composition_features.brightness` | true |\n"
    "| `ppu_policy.display_capabilities.composition_features.bg_mosaic` | true |\n"
    "| `ppu_policy.display_capabilities.composition_features.obj_mosaic` | true |\n"
    "| `ppu_policy.display_capabilities.composition_features.forced_blank` | true |\n"
    "| `ppu_policy.display_capabilities.composition_features.classification` | project-selected |\n"
    "| `ppu_policy.display_capabilities.composition_features.hardware_qualification` | UNKNOWN |")
  string(JOIN "" composition_features_table ${composition_features_table})
  gbl__profile_doc_exact_policy_table("${normalized_text}"
    "## PPU composition features" ppu_policy.display_capabilities.composition_features
    "${composition_features_table}" ppu-capability-doc)

  set(capability_rows)
  set(row "| `ppu_policy.display_capabilities.modes[0]` | partial | BG0 text: 256x256, 512x256, 256x512, 512x512; BG1 text: 256x256, 512x256, 256x512, 512x512; BG2 text: 256x256, 512x256, 256x512, 512x512; BG3 text: 256x256, 512x256, 256x512, 512x512 |")
  string(REPLACE ";" "\\;" row "${row}")
  list(APPEND capability_rows "${row}")
  set(row "| `ppu_policy.display_capabilities.modes[1]` | partial | BG0 text: 256x256, 512x256, 256x512, 512x512; BG1 text: 256x256, 512x256, 256x512, 512x512; BG2 affine: 128x128, 256x256, 512x512, 1024x1024 |")
  string(REPLACE ";" "\\;" row "${row}")
  list(APPEND capability_rows "${row}")
  set(row "| `ppu_policy.display_capabilities.modes[2]` | partial | BG2 affine: 128x128, 256x256, 512x512, 1024x1024; BG3 affine: 128x128, 256x256, 512x512, 1024x1024 |")
  string(REPLACE ";" "\\;" row "${row}")
  list(APPEND capability_rows "${row}")
  list(APPEND capability_rows
    "| `ppu_policy.display_capabilities.modes[3]` | partial | BG2 direct-color-bitmap: 240x160 |"
    "| `ppu_policy.display_capabilities.modes[4]` | partial | BG2 indexed-bitmap: 240x160 |"
    "| `ppu_policy.display_capabilities.modes[5]` | partial | BG2 direct-color-bitmap: 160x128 |")
  gbl__profile_doc_policy_table("${normalized_text}"
    "## PPU display capabilities" ppu_policy.display_capabilities 6 capability_rows
    "Mode;Implementation status;Background sources and dimensions" ppu-capability-doc 3)

  set(raw_output_rows
    "| `ppu_policy.display_capabilities.raw_output.format` | BGR555_LE |"
    "| `ppu_policy.display_capabilities.raw_output.width` | 240 |"
    "| `ppu_policy.display_capabilities.raw_output.height` | 160 |"
    "| `ppu_policy.display_capabilities.raw_output.stride_bytes` | 480 |"
    "| `ppu_policy.display_capabilities.raw_output.byte_length` | 76800 |")
  gbl__profile_doc_policy_table("${normalized_text}"
    "## PPU raw output" ppu_policy.display_capabilities.raw_output 5 raw_output_rows
    "Setting;Value" ppu-capability-doc 2)

  set(access_rows
    "| `ppu_policy.video_memory_access.matrix` | project-selected | UNKNOWN | cpu, dma | pram, vram, oam | drawing, hblank, vblank, forced-blank | existing-bus-behavior | 0 | `bus_policy.request_cycles` |")
  gbl__profile_doc_policy_table("${normalized_text}"
    "## Video memory access policy" ppu_policy.video_memory_access 1 access_rows
    "Setting;Classification;Hardware qualification;Masters;Regions;States;Availability;PPU extra cycles;Cost reference"
    ppu-access-policy-doc 9)
endfunction()

function(gbl__profile_doc_audio_sync normalized_text profile)
  set(audio_rows)
  foreach(pair IN LISTS GBL_PROFILE_AUDIO_POLICY_ROWS)
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 key)
    gbl_json_type(value_type "${profile}" audio_policy "${key}")
    if("${value_type}" STREQUAL "MISSING")
      set(value "<missing>")
    else()
      string(JSON value ERROR_VARIABLE value_error GET
        "${profile}" audio_policy "${key}")
      if(NOT "${value_error}" STREQUAL "NOTFOUND" AND
          NOT "${value_error}" STREQUAL "")
        set(value "<invalid>")
      endif()
    endif()
    string(REPLACE ";" "\\;" value_cell "${value}")
    list(APPEND audio_rows "| `audio_policy.${key}` | ${value_cell} |")
  endforeach()
  list(LENGTH audio_rows audio_count)
  gbl__profile_doc_policy_table("${normalized_text}"
    "## Audio behavior policy" audio_policy ${audio_count} audio_rows
    "Setting;Value" audio-policy-doc 2)
  set(expected_audio_table "| Setting | Value |\n| --- | --- |\n")
  foreach(row IN LISTS audio_rows)
    string(APPEND expected_audio_table "${row}\n")
  endforeach()
  gbl__profile_doc_exact_policy_table("${normalized_text}"
    "## Audio behavior policy" audio_policy "${expected_audio_table}"
    audio-policy-doc)

  set(soundbias_rows)
  foreach(field IN LISTS GBL_PROFILE_AUDIO_SOUNDBIAS_FIELDS)
    gbl_json_type(value_type "${profile}" audio_policy soundbias "${field}")
    if("${value_type}" STREQUAL "MISSING")
      set(value "<missing>")
    else()
      string(JSON value ERROR_VARIABLE value_error GET
        "${profile}" audio_policy soundbias "${field}")
      if(NOT "${value_error}" STREQUAL "NOTFOUND" AND
          NOT "${value_error}" STREQUAL "")
        set(value "<invalid>")
      endif()
    endif()
    list(APPEND soundbias_rows
      "| `audio_policy.soundbias.${field}` | ${value} |")
  endforeach()
  list(LENGTH soundbias_rows soundbias_count)
  gbl__profile_doc_policy_table("${normalized_text}"
    "## SOUNDBIAS policy" audio_policy.soundbias ${soundbias_count}
    soundbias_rows "Setting;Value" audio-policy-doc 2)

  set(register_rows)
  gbl_json_type(registers_type "${profile}" audio_policy registers)
  if("${registers_type}" STREQUAL "ARRAY")
    string(JSON register_count LENGTH "${profile}" audio_policy registers)
    set(register_fields name address retained_mask read_mask action_mask effect)
    if(register_count GREATER 0)
      math(EXPR last_register "${register_count}-1")
      foreach(index RANGE 0 ${last_register})
        set(values)
        foreach(field IN LISTS register_fields)
          string(JSON value ERROR_VARIABLE value_error GET
            "${profile}" audio_policy registers "${index}" "${field}")
          if(NOT "${value_error}" STREQUAL "NOTFOUND" AND
              NOT "${value_error}" STREQUAL "")
            set(value "<missing>")
          endif()
          list(APPEND values "${value}")
        endforeach()
        list(GET values 0 name)
        list(GET values 1 address)
        list(GET values 2 retained_mask)
        list(GET values 3 read_mask)
        list(GET values 4 action_mask)
        list(GET values 5 effect)
        list(APPEND register_rows
          "| `audio_policy.registers.${name}` | `${name}` | `${address}` | `${retained_mask}` | `${read_mask}` | `${action_mask}` | ${effect} |")
      endforeach()
    endif()
  else()
    set(register_count 0)
  endif()
  gbl__profile_doc_policy_table("${normalized_text}"
    "## Audio register policy" audio_policy.registers ${register_count}
    register_rows
    "Setting;Register;Address;Retained mask;Read mask;Action mask;Projected effect"
    audio-policy-doc 7)
endfunction()

# gbl__profile_doc_sync(<doc-file> <profile-json> <json-ids> <version-text>): the canonical
# profile narrative must agree with the JSON. Normalize once before any
# metadata, heading, section, or table lookup. <version-text> is empty when the
# JSON version is not valid (bad-version already reported).
function(gbl__profile_doc_sync docfile profile json_ids version)
  if(NOT EXISTS "${docfile}" OR IS_DIRECTORY "${docfile}")
    gbl_finding(profile doc-no-section "${docfile}: unreadable")
    return()
  endif()
  # file(READ), never file(STRINGS): lines may hold semicolons or brackets.
  file(READ "${docfile}" raw_text)
  set(raw_text "\n${raw_text}")
  gbl__profile_doc_normalize(normalized_text "${raw_text}")
  unset(raw_text)

  # Reject unsupported HTML in the canonical profile document before finding
  # any synchronized sections. This catches blocks that begin before a section
  # and span across its heading or table.
  gbl__profile_doc_unsupported_html(unsupported_markup "${normalized_text}")
  if(NOT "${unsupported_markup}" STREQUAL "")
    gbl_finding(profile bus-policy-doc
      "unsupported raw HTML ${unsupported_markup}")
    return()
  endif()

  # Count complete standalone lines instead of matching one substring; a
  # second contradictory visible version must not be silently ignored.
  set(remaining "${normalized_text}")
  set(version_count 0)
  set(document_version "")
  while(TRUE)
    string(FIND "${remaining}" "\n" line_end)
    if(line_end EQUAL -1)
      set(line "${remaining}")
      set(remaining "")
      set(last_line TRUE)
    else()
      string(SUBSTRING "${remaining}" 0 ${line_end} line)
      math(EXPR next_line "${line_end}+1")
      string(SUBSTRING "${remaining}" ${next_line} -1 remaining)
      set(last_line FALSE)
    endif()
    if("${line}" MATCHES "^profile_version: ([0-9]+)$")
      math(EXPR version_count "${version_count}+1")
      set(document_version "${CMAKE_MATCH_1}")
    endif()
    if(last_line)
      break()
    endif()
  endwhile()

  if(version_count EQUAL 0)
    gbl_finding(profile doc-version-mismatch "no profile_version line")
  elseif(version_count GREATER 1)
    gbl_finding(profile doc-version-mismatch
      "expected exactly one profile_version line, found ${version_count}")
  elseif(NOT "${version}" STREQUAL "")
    if(NOT "${document_version}" STREQUAL "${version}")
      gbl_finding(profile doc-version-mismatch "document ${document_version}, profile ${version}")
    endif()
  endif()

  foreach(heading IN ITEMS "## Bus policy" "## Initialization policy" "## Read policy")
    string(FIND "${normalized_text}" "${heading}" heading_at)
    if(heading_at EQUAL -1)
      gbl_finding(profile bus-policy-doc "missing ${heading} section")
    endif()
  endforeach()
  foreach(heading IN ITEMS "## CPU direct-start policy" "## CPU instruction scope")
    string(FIND "${normalized_text}" "${heading}" heading_at)
    if(heading_at EQUAL -1)
      gbl_finding(profile cpu-policy-doc "missing ${heading} section")
    endif()
  endforeach()

  gbl_json_string(bus_source "${profile}" bus_policy source)
  gbl_json_string(bus_cite "${profile}" bus_policy cite)
  string(REPLACE ";" "\\;" bus_cite_row "${bus_cite}")
  set(bus_rows)
  foreach(k IN LISTS GBL_PROFILE_BUS_REQUEST_KEYS)
    if(k STREQUAL "rom")
      set(request_cycles "contextual-waitcnt-halfword-beats")
    else()
      set(request_cycles 1)
    endif()
    list(APPEND bus_rows "| bus_policy.request_cycles.${k} | ${request_cycles} | ${bus_source} | ${bus_cite_row} |")
  endforeach()
  list(APPEND bus_rows
    "| bus_policy.widths_bytes | [1, 2, 4] | ${bus_source} | ${bus_cite_row} |"
    "| bus_policy.access_classes | [nonsequential, sequential] | ${bus_source} | ${bus_cite_row} |"
    "| bus_policy.internal_cycle_unit | 1 | ${bus_source} | ${bus_cite_row} |")
  gbl__profile_doc_policy_table("${normalized_text}" "## Bus policy" bus_policy 12 bus_rows)

  gbl_json_string(init_source "${profile}" initialization_policy source)
  gbl_json_string(init_cite "${profile}" initialization_policy cite)
  string(REPLACE ";" "\\;" init_cite_row "${init_cite}")
  set(init_rows)
  foreach(k IN ITEMS memory_fill register_fill master_cycles open_bus)
    list(APPEND init_rows "| initialization_policy.${k} | 0 | ${init_source} | ${init_cite_row} |")
  endforeach()
  gbl__profile_doc_policy_table("${normalized_text}" "## Initialization policy"
    initialization_policy 4 init_rows)

  gbl_json_string(read_source "${profile}" read_policy source)
  gbl_json_string(read_cite "${profile}" read_policy cite)
  string(REPLACE ";" "\\;" read_cite_row "${read_cite}")
  set(read_rows
    "| read_policy.absent_bios | current-open-bus | ${read_source} | ${read_cite_row} |"
    "| read_policy.unsupported_io | current-open-bus | ${read_source} | ${read_cite_row} |"
    "| read_policy.rom_tail | current-open-bus | ${read_source} | ${read_cite_row} |"
    "| read_policy.latch_update | mapped-read-width-replicated | ${read_source} | ${read_cite_row} |"
    "| read_policy.write_latch | preserve | ${read_source} | ${read_cite_row} |")
  gbl__profile_doc_policy_table("${normalized_text}" "## Read policy" read_policy 5 read_rows)

  set(cpu_rows
    "| cpu_policy.entry_address | 134217728 | IMPL | ${GBL_PROFILE_CPU_CITE} |"
    "| cpu_policy.cpsr | 223 | IMPL | ${GBL_PROFILE_CPU_CITE} |"
    "| cpu_policy.register_bank_fill | 0 | IMPL | ${GBL_PROFILE_CPU_CITE} |"
    "| cpu_policy.spsr_fill | 0 | IMPL | ${GBL_PROFILE_CPU_CITE} |"
    "| cpu_policy.user_system_spsr | absent | REF | ${GBL_PROFILE_CPU_ARCH_CITE} |"
    "| cpu_policy.register_banks | User/System share r8-r14, FIQ has private r8-r14, IRQ/SVC/ABT/UND each have private r13-r14, and FIQ/IRQ/SVC/ABT/UND have SPSRs. | REF | ${GBL_PROFILE_CPU_ARCH_CITE} |"
    "| cpu_policy.pipeline | empty | IMPL | ${GBL_PROFILE_CPU_CITE} |"
    "| cpu_policy.arm_pc_offset | 8 | REF | ${GBL_PROFILE_CPU_ARCH_CITE} |"
    "| cpu_policy.thumb_pc_offset | 4 | REF | ${GBL_PROFILE_CPU_ARCH_CITE} |"
    "| cpu_policy.timing | unit-preflight-uniform-bus-plus-core-internal-work | IMPL | ${GBL_PROFILE_CPU_CITE} |"
    "| cpu_policy.exception_entry_cycles | 1 | IMPL | ${GBL_PROFILE_CPU_CITE} |"
    "| cpu_policy.bios_none_stop | after-entry-before-vector-fetch | IMPL | ${GBL_PROFILE_CPU_CITE} |"
    "| cpu_policy.execution | single-interpreter | IMPL | ${GBL_PROFILE_CPU_CITE} |"
    "| cpu_policy.pc_operand_reads | arm-current+8-register-shift-rm-and-store+12-thumb-current+4 | IMPL | ${GBL_PROFILE_CPU_CITE} |"
    "| cpu_policy.internal_work | ddi0029g-6.5-6.6-6.20-cost-table | IMPL | ${GBL_PROFILE_CPU_CITE} |"
    "| cpu_policy.multiply_flag_policy | preserve-short-c-long-cv | IMPL | ${GBL_PROFILE_CPU_CITE} |"
    "| cpu_policy.loaded_pc_boundary | align-mask-wrap-refill | IMPL | ${GBL_PROFILE_CPU_CITE} |"
    "| cpu_policy.unpredictable_operands | named-stop | IMPL | ${GBL_PROFILE_CPU_CITE} |"
    "| cpu_policy.extension_encoding | later-architecture-stop | IMPL | ${GBL_PROFILE_CPU_CITE} |"
    "| cpu_policy.address_arithmetic | data-modulo32-static-fetch-checked | IMPL | ${GBL_PROFILE_CPU_CITE} |")
  gbl__profile_doc_policy_table("${normalized_text}" "## CPU direct-start policy"
    cpu_policy 20 cpu_rows "Setting;Value;Label;Citation" cpu-policy-doc)

  set(cpu_scope_rows)
  set(scope_index 0)
  foreach(scope IN LISTS GBL_PROFILE_CPU_SUPPORTED)
    math(EXPR scope_index "${scope_index}+1")
    list(APPEND cpu_scope_rows
      "| cpu_scope.supported.${scope_index} | ${scope} | IMPL | ${GBL_PROFILE_CPU_CITE} |")
  endforeach()
  set(scope_index 0)
  foreach(scope IN LISTS GBL_PROFILE_CPU_EXCLUDED)
    math(EXPR scope_index "${scope_index}+1")
    list(APPEND cpu_scope_rows
      "| cpu_scope.excluded.${scope_index} | ${scope} | IMPL | ${GBL_PROFILE_CPU_CITE} |")
  endforeach()
  list(LENGTH cpu_scope_rows cpu_scope_count)
  gbl__profile_doc_policy_table("${normalized_text}" "## CPU instruction scope"
    cpu_scope ${cpu_scope_count} cpu_scope_rows "Scope;Opcode / operand forms;Label;Citation" cpu-policy-doc)

  set(cpu_stop_rows)
  foreach(pair IN ITEMS "unpredictable|unpredictable-instruction"
      "later-architecture|later-architecture-instruction"
      "undefined|no-bios-undefined" "absent-coprocessor|no-bios-coprocessor"
      "swi|no-bios-swi" "static-target-refusal|unsupported-instruction")
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 key)
    list(GET fields 1 reason)
    list(APPEND cpu_stop_rows
      "| cpu_stop.${key} | ${reason} | IMPL | ${GBL_PROFILE_CPU_CITE} |")
  endforeach()
  gbl__profile_doc_policy_table("${normalized_text}" "## CPU stop classifications"
    cpu_stop 6 cpu_stop_rows "Setting;Value;Label;Citation" cpu-policy-doc)

  string(REPLACE ";" "\\;" irq_cite_row "${GBL_PROFILE_IRQ_POLICY_CITE}")
  set(irq_rows
    "| irq_input_power_policy.if_ie_mask | 16383 | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.ime_mask | 1 | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.cpsr_i_mask | 128 | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.timer0_if_bit | 8 | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.keypad_if_bit | 4096 | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.keyinput_mask | 1023 | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.keycnt_mask | 50175 | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.sample_boundary | completed-instruction-effects-and-events-before-next-instruction | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.initial_boundary | after-refill-before-first-instruction | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.entry_internal_cycles | 1 | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.no_bios_irq_stop | after-entry-before-vector-fetch | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.timer_event_priorities | [0, 1, 2, 3] | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.keypad_event_priority | 4 | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.input_event_capacity | 256 | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.key_polarity | active-low | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.keycnt_empty_selection | false | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.keycnt_effective_edge | false-to-true | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.if_ack_while_held | stays-clear-until-condition-falls-then-rises | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.halt_wake_condition | IF-and-IE | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.halt_wake_mask_independent | [IME, CPSR.I] | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.power_transition_cost | internal-only | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.stop_policy | terminal-guest-stop-no-bios-wake | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.stop_cause_code | 3 | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.deadlock_policy | distinct-guest-stop | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.deadlock_cause_code | 4 | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.timeout_policy | cycle-budget-timeout | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.timeout_reason_code | 2 | IMPL | ${irq_cite_row} |"
    "| irq_input_power_policy.qualification | project-model-only | IMPL | ${irq_cite_row} |")
  gbl__profile_doc_policy_table("${normalized_text}"
    "## IRQ, input, and power policy" irq_input_power_policy 28 irq_rows
    "Setting;Value;Label;Citation" irq-policy-doc)

  set(irq_source_rows
    "| irq_input_power_policy.source_facts.arm_exception_entry | ARM7TDMI exception state and entry behavior | REF | ${GBL_PROFILE_IRQ_ARM_CITE} |"
    "| irq_input_power_policy.source_facts.gba_devices_and_power | GBA keypad, IRQ, HALT, and STOP behavior | REF | ${GBL_PROFILE_IRQ_GBA_CITE} |")
  gbl__profile_doc_policy_table("${normalized_text}"
    "## IRQ/input/power policy sources" irq_input_power_policy.source_facts
    2 irq_source_rows "Fact;Evidence;Label;Citation" irq-policy-doc)

  gbl__profile_doc_timing_sync("${normalized_text}" "${profile}")
  gbl__profile_doc_ppu_sync("${normalized_text}" "${profile}")
  gbl__profile_doc_audio_sync("${normalized_text}" "${profile}")

  string(FIND "${normalized_text}" "\n## Unmodeled\n" at)
  set(doc_ids "")
  if(NOT at EQUAL -1)
    string(LENGTH "\n## Unmodeled" hlen)
    math(EXPR from "${at}+${hlen}")
    string(SUBSTRING "${normalized_text}" ${from} -1 section)
    string(FIND "${section}" "\n## " nxt)
    if(NOT nxt EQUAL -1)
      string(SUBSTRING "${section}" 0 ${nxt} section)
    endif()
    gbl__profile_doc_unmodeled_table(table "${section}")
    string(REGEX MATCHALL "\n[ \t]*\\|[ \t]*`[^`]+`" rows "${table}")
    foreach(row IN LISTS rows)
      string(FIND "${row}" "`" id_start)
      math(EXPR id_start "${id_start}+1")
      string(SUBSTRING "${row}" ${id_start} -1 id_tail)
      string(FIND "${id_tail}" "`" id_end)
      if(NOT id_end EQUAL -1)
        string(SUBSTRING "${id_tail}" 0 ${id_end} rid)
        list(APPEND doc_ids "${rid}")
      endif()
    endforeach()
  endif()
  if("${doc_ids}" STREQUAL "")
    gbl_finding(profile doc-no-section "## Unmodeled")
    return()
  endif()

  list(LENGTH json_ids expected_id_count)
  list(LENGTH doc_ids actual_id_count)
  if(actual_id_count GREATER expected_id_count)
    gbl_finding(profile doc-extra-id "expected ${expected_id_count} rows, found ${actual_id_count}")
  elseif(actual_id_count LESS expected_id_count)
    gbl_finding(profile doc-missing-id "expected ${expected_id_count} rows, found ${actual_id_count}")
  endif()
  set(seen_ids "")
  foreach(id IN LISTS doc_ids)
    if("${id}" IN_LIST seen_ids)
      gbl_finding(profile doc-extra-id "duplicate id ${id}")
    endif()
    list(APPEND seen_ids "${id}")
  endforeach()
  foreach(id IN LISTS json_ids)
    if(NOT "${id}" IN_LIST doc_ids)
      gbl_finding(profile doc-missing-id "${id}")
    endif()
  endforeach()
  foreach(id IN LISTS doc_ids)
    if(NOT "${id}" IN_LIST json_ids)
      gbl_finding(profile doc-extra-id "${id}")
    endif()
  endforeach()
endfunction()

# ---- script-mode entry (inert when include()d) --------------------------------
if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
  foreach(var PROFILE DOC REGISTRY VOCAB)
    if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
      message(FATAL_ERROR "profile: usage: -D${var}= is required")
    endif()
  endforeach()

  gbl_contract_read(V "${VOCAB}")
  gbl_registry_read(R "${REGISTRY}" V)
  gbl_json_load(doc "${PROFILE}")

  # Focused mutation controls target the production policy validator directly.
  # The ordinary lint and clean-copy controls still exercise the full profile.
  if(DEFINED GBL_PROFILE_AUDIO_ONLY AND GBL_PROFILE_AUDIO_ONLY)
    gbl__profile_audio_policy("${doc}")
    if(NOT EXISTS "${DOC}" OR IS_DIRECTORY "${DOC}")
      gbl_finding(profile audio-policy-doc "${DOC}: unreadable")
    else()
      file(READ "${DOC}" audio_raw_text)
      set(audio_raw_text "\n${audio_raw_text}")
      gbl__profile_doc_normalize(audio_normalized_text "${audio_raw_text}")
      gbl__profile_doc_audio_sync("${audio_normalized_text}" "${doc}")
    endif()
    gbl_findings_finish(profile "ok (agb audio policy)")
    return()
  endif()

  if(DEFINED GBL_PROFILE_PPU_ONLY AND GBL_PROFILE_PPU_ONLY)
    gbl__profile_ppu_policy("${doc}")
    if(NOT EXISTS "${DOC}" OR IS_DIRECTORY "${DOC}")
      gbl_finding(profile ppu-capability-doc "${DOC}: unreadable")
    else()
      file(READ "${DOC}" ppu_raw_text)
      set(ppu_raw_text "\n${ppu_raw_text}")
      gbl__profile_doc_normalize(ppu_normalized_text "${ppu_raw_text}")
      gbl__profile_doc_ppu_sync("${ppu_normalized_text}" "${doc}")
    endif()
    gbl_findings_finish(profile "ok (agb PPU policy)")
    return()
  endif()

  # Top-level scalar fields.
  gbl_json_string(schema "${doc}" schema)
  if("${schema}" STREQUAL "<missing>")
    gbl_finding(profile missing-field "schema")
  elseif(NOT "${schema}" STREQUAL "gamebloke.profile/1")
    gbl_finding(profile bad-schema "${PROFILE}")
  endif()

  set(version "")
  gbl_json_type(vty "${doc}" profile_version)
  if("${vty}" STREQUAL "MISSING" OR "${vty}" STREQUAL "NULL")
    gbl_finding(profile missing-field "profile_version")
  elseif(NOT "${vty}" STREQUAL "NUMBER")
    gbl_finding(profile bad-version "profile_version")
  else()
    string(JSON vtext GET "${doc}" profile_version)
    if("${vtext}" MATCHES "^[1-9][0-9]*$")
      set(version "${vtext}")
    else()
      gbl_finding(profile bad-version "profile_version")
    endif()
  endif()

  foreach(field id machine)
    gbl_json_string(value "${doc}" ${field})
    if("${value}" STREQUAL "<missing>")
      gbl_finding(profile missing-field "${field}")
    elseif(NOT "${value}" STREQUAL "agb")
      gbl_finding(profile bad-machine "${field}")
    endif()
  endforeach()

  gbl_json_string(boot "${doc}" boot)
  if("${boot}" STREQUAL "<missing>")
    gbl_finding(profile missing-field "boot")
  elseif(NOT "${boot}" STREQUAL "direct-start")
    gbl_finding(profile bad-boot "boot")
  endif()

  gbl_json_string(bios "${doc}" bios_provider)
  if("${bios}" STREQUAL "<missing>")
    gbl_finding(profile missing-field "bios_provider")
  else()
    gbl_bios_provider_valid(bios_ok "${bios}")
    if(NOT bios_ok)
      gbl_finding(profile bad-bios-provider "bios_provider")
    endif()
  endif()

  gbl_json_type(cart_ty "${doc}" cart)
  if("${cart_ty}" STREQUAL "MISSING" OR "${cart_ty}" STREQUAL "NULL")
    gbl_finding(profile missing-field "cart")
  elseif(NOT "${cart_ty}" STREQUAL "OBJECT")
    gbl_finding(profile bad-cart "cart: not an object")
  else()
    string(JSON cart_len LENGTH "${doc}" cart)
    if(cart_len EQUAL 0)
      gbl_finding(profile missing-field "cart")
    endif()
    gbl_json_string(kind "${doc}" cart kind)
    if(NOT "${kind}" STREQUAL "rom-only")
      gbl_finding(profile bad-cart "cart.kind")
    endif()
  endif()

  # Timing and unmodeled.
  gbl_json_type(timing_ty "${doc}" timing)
  if("${timing_ty}" STREQUAL "MISSING" OR "${timing_ty}" STREQUAL "NULL")
    gbl_finding(profile missing-field "timing")
  elseif(NOT "${timing_ty}" STREQUAL "OBJECT")
    gbl_finding(profile missing-field "timing: not an object")
  else()
    string(JSON timing_len LENGTH "${doc}" timing)
    if(timing_len EQUAL 0)
      gbl_finding(profile missing-field "timing")
    endif()
    gbl__profile_timing("${doc}")
  endif()

  gbl__profile_ppu_policy("${doc}")
  gbl__profile_audio_policy("${doc}")
  gbl__profile_bus_policy("${doc}")
  gbl__profile_waitcnt_policy("${doc}")
  gbl__profile_dma_policy("${doc}")
  gbl__profile_timing_model("${doc}")
  gbl__profile_initialization_policy("${doc}")
  gbl__profile_read_policy("${doc}")
  gbl__profile_cpu_policy("${doc}")
  gbl__profile_irq_input_power_policy("${doc}")

  set(json_ids "")
  gbl_json_length(um_len "${doc}" unmodeled)
  if(um_len LESS 1)
    gbl_finding(profile missing-field "unmodeled")
    set(um_len 0)
  else()
    gbl__profile_unmodeled(json_ids "${doc}")
  endif()

  gbl__profile_doc_sync("${DOC}" "${doc}" "${json_ids}" "${version}")

  gbl_findings_finish(profile "ok (agb v${version}, 11 timing constants, ${um_len} unmodeled)")
endif()
