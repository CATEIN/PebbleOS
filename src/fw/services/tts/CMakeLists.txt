# SPDX-FileCopyrightText: 2026 CATEIN
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(
  sam/female.c
  sam/reciter.c
  sam/render.c
  sam/sam.c
  tts.c
  tts_synth.c
  tts_text.c
)
# SAM streams its samples to a sink instead of filling a 10 s buffer
pbl_library_compile_definitions(SAM_STREAM)
