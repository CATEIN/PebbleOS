#ifndef RENDER_H
#define RENDER_H

void Render();
void SetMouthThroat(unsigned char mouth, unsigned char throat);
void SetSamFemale(int on); // SAM renderer: 0 SAM, 1 female tables, 2 Teto-like tables

// SAM_STREAM builds (microcontrollers): SAM's renderer streams unsigned 8-bit
// samples to the sink instead of filling the 10 s buffer. SamStreamFlush()
// sends the rest after SAMMain(); SAMMain() resets the stream.
typedef void (*SamSink)(const unsigned char *samples, int n);
void SetSamSink(SamSink sink);
void SamStreamReset();
void SamStreamFlush();

// Firmware: set to stop the current SAMMain() early (checked once per tick)
extern volatile int samAbort;

#endif
