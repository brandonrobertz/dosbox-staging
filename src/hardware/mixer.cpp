/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

// #define DEBUG 1

#include "mixer.h"

#include <cstdint>
#include <cmath>
#include <cstring>
#include <sys/types.h>

#include <algorithm>
#include <array>
#include <atomic>

#if defined (WIN32)
//Midi listing
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

#include <SDL.h>

#include "mem.h"
#include "pic.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "cross.h"
#include "string_utils.h"
#include "mapper.h"
#include "hardware.h"
#include "programs.h"
#include "midi.h"

#define MIXER_SSIZE 4

//#define MIXER_SHIFT 14
//#define MIXER_REMAIN ((1<<MIXER_SHIFT)-1)

#define MIXER_VOLSHIFT 13

#define FREQ_SHIFT 14
#define FREQ_NEXT ( 1 << FREQ_SHIFT)
#define FREQ_MASK ( FREQ_NEXT -1 )

#define TICK_SHIFT 24
#define TICK_NEXT ( 1 << TICK_SHIFT)
#define TICK_MASK (TICK_NEXT -1)

// Over how many milliseconds will we permit a signal to grow from
// zero up to peak amplitude? (recommended 10 to 20ms)
#define ENVELOPE_MAX_EXPANSION_OVER_MS 15u

// Regardless if the signal needed to be eveloped or not, how long
// should the envelope monitor the initial signal? (recommended > 5s)
#define ENVELOPE_EXPIRES_AFTER_S 10u

template <class T, size_t ROWS, size_t COLS>
using matrix = std::array<std::array<T, COLS>, ROWS>;

static constexpr int16_t MIXER_CLIP(const Bits SAMP)
{
	if (SAMP <= MIN_AUDIO) return MIN_AUDIO;
	if (SAMP >= MAX_AUDIO) return MAX_AUDIO;
	
	return static_cast<int16_t>(SAMP);
}

struct mixer_t {
	matrix<int32_t, MIXER_BUFSIZE, 2> work;
	//Write/Read pointers for the buffer
	std::atomic<uint32_t> pos;
	std::atomic<uint32_t> done;
	std::atomic<uint32_t> needed;
	std::atomic<uint32_t> min_needed;
	std::atomic<uint32_t> max_needed;
	// For every millisecond tick how many samples need to be generated
	std::atomic<uint32_t> tick_add;
	uint32_t tick_counter;
	std::array<float, 2> mastervol;
	MixerChannel *channels;
	bool nosound;
	uint32_t freq;
	uint16_t blocksize; // matches SDL AudioSpec.samples type
	// Note: As stated earlier, all sdl code shall rather be in sdlmain
	SDL_AudioDeviceID sdldevice;
	mixer_t()
	        : work(),
			  pos(0),
	          done(0),
	          needed(0),
	          min_needed(0),
	          max_needed(0),
	          tick_add(0),
			  tick_counter(0),
			  mastervol{1.0f, 1.0f},
			  channels(nullptr),
			  nosound(false),
	          freq(0),
			  blocksize(0),
			  sdldevice(0)
	{}
};

static struct mixer_t mixer;

Bit8u MixTemp[MIXER_BUFSIZE];

MixerChannel::MixerChannel(MIXER_Handler _handler,
                           [[maybe_unused]] Bitu _freq,
                           const char *_name)
        : name(_name),
          done(0),
          envelope(name),
          handler(_handler)
{}

MixerChannel * MIXER_AddChannel(MIXER_Handler handler, Bitu freq, const char * name) {
	MixerChannel * chan=new MixerChannel(handler, freq, name);
	chan->next=mixer.channels;
	chan->SetFreq(freq); // also enables 'interpolate' if needed
	chan->SetScale(1.0);
	chan->SetVolume(1, 1);
	chan->MapChannels(0, 1);
	chan->Enable(false);
	mixer.channels=chan;

	const auto mix_rate = mixer.freq;
	const auto chan_rate = chan->GetSampleRate();
	if (chan_rate == mix_rate)
		LOG_MSG("MIXER: %s channel operating at %u Hz without resampling",
		        name, chan_rate);
	else
		LOG_MSG("MIXER: %s channel operating at %u Hz and %s to the output rate",
		        name, chan_rate,
		        chan_rate > mix_rate ? "downsampling" : "upsampling");
	return chan;
}

MixerChannel * MIXER_FindChannel(const char * name) {
	MixerChannel * chan=mixer.channels;
	while (chan) {
		if (!strcasecmp(chan->name,name)) break;
		chan=chan->next;
	}
	return chan;
}

void MIXER_DelChannel(MixerChannel* delchan) {
	MixerChannel * chan=mixer.channels;
	MixerChannel * * where=&mixer.channels;
	while (chan) {
		if (chan==delchan) {
			*where=chan->next;
			delete delchan;
			return;
		}
		where=&chan->next;
		chan=chan->next;
	}
}

static void MIXER_LockAudioDevice()
{
	SDL_LockAudioDevice(mixer.sdldevice);
}

static void MIXER_UnlockAudioDevice()
{
	SDL_UnlockAudioDevice(mixer.sdldevice);
}

void MixerChannel::RegisterLevelCallBack(apply_level_callback_f cb)
{
	apply_level = cb;
	const AudioFrame level{volmain[0], volmain[1]};
	apply_level(level);
}

void MixerChannel::UpdateVolume()
{
	// Don't scale by volmain[] if the level is being managed by the source
	const float level_l = apply_level ? 1 : volmain[0];
	const float level_r = apply_level ? 1 : volmain[1];
	volmul[0] = (Bits)((1 << MIXER_VOLSHIFT) * scale[0] * level_l * mixer.mastervol[0]);
	volmul[1] = (Bits)((1 << MIXER_VOLSHIFT) * scale[1] * level_r * mixer.mastervol[1]);
}

void MixerChannel::SetVolume(float _left,float _right) {
	// Allow unconstrained user-defined values
	volmain[0] = _left;
	volmain[1] = _right;

	if (apply_level) {
		const AudioFrame level{_left, _right};
		apply_level(level);
	}
	UpdateVolume();
}

void MixerChannel::SetScale(float f) {
	SetScale(f, f);
}

void MixerChannel::SetScale(float _left, float _right) {
	// Constrain application-defined volume between 0% and 100%
	const float min_volume(0.0);
	const float max_volume(1.0);
	_left  = clamp(_left,  min_volume, max_volume);
	_right = clamp(_right, min_volume, max_volume);
	if (scale[0] != _left || scale[1] != _right) {
		scale[0] = _left;
		scale[1] = _right;
		UpdateVolume();
#ifdef DEBUG
		LOG_MSG("MIXER %-7s channel: application changed left and right volumes to %3.0f%% and %3.0f%%, respectively",
		        name, scale[0] * 100, scale[1] * 100);
#endif
	}
}

void MixerChannel::MapChannels(Bit8u _left, Bit8u _right) {
	// Constrain mapping to the 0 (left) and 1 (right) channel indexes
	const Bit8u min_channel(0);
	const Bit8u max_channel(1);
	_left =  clamp(_left,  min_channel, max_channel);
	_right = clamp(_right, min_channel, max_channel);
	if (channel_map[0] != _left || channel_map[1] != _right) {
		channel_map[0] = _left;
		channel_map[1] = _right;
#ifdef DEBUG
		LOG_MSG("MIXER %-7s channel: application changed audio-channel mapping to left=>%s and right=>%s",
		        name,
		        channel_map[0] == 0 ? "left" : "right",
		        channel_map[1] == 0 ? "left" : "right");
#endif
	}
}

void MixerChannel::Enable(const bool should_enable)
{
	// Is the channel already in the desired state?
	if (is_enabled == should_enable)
		return;

	// Lock the channel before changing states
	MIXER_LockAudioDevice();

	// Prepare the channel to accept samples
	if (should_enable) {
		freq_counter = 0u;
		// Don't start with a deficit
		if (done < mixer.done)
			done = mixer.done.load();

		// Prepare the channel to go dormant
	} else {
		// Clear the current counters and sample values to
		// start clean if/when this channel is re-enabled.
		// Samples can be buffered into disable channels, so
		// we don't perform this zero'ing in the enable phase.
		done = 0u;
		needed = 0u;
		prev_sample[0] = 0;
		prev_sample[1] = 0;
		next_sample[0] = 0;
		next_sample[1] = 0;
	}
	is_enabled = should_enable;
	MIXER_UnlockAudioDevice();
}

void MixerChannel::SetFreq(Bitu freq)
{
	if (!freq) {
		// If the channel rate is zero, then avoid resampling by running
		// the channel at the same rate as the mixer
		assert(mixer.freq > 0);
		freq = mixer.freq;
	}
	freq_add = (freq << FREQ_SHIFT) / mixer.freq;
	interpolate = (freq != mixer.freq);
	sample_rate = static_cast<uint32_t>(freq);
	envelope.Update(sample_rate, peak_amplitude,
	                ENVELOPE_MAX_EXPANSION_OVER_MS, ENVELOPE_EXPIRES_AFTER_S);
}

bool MixerChannel::IsInterpolated() const
{
	return interpolate;
}

uint32_t MixerChannel::GetSampleRate() const
{
	return sample_rate;
}

void MixerChannel::SetPeakAmplitude(const uint32_t peak)
{
	peak_amplitude = peak;
	envelope.Update(sample_rate, peak_amplitude,
	                ENVELOPE_MAX_EXPANSION_OVER_MS, ENVELOPE_EXPIRES_AFTER_S);
}

void MixerChannel::Mix(Bitu _needed) {
	needed=_needed;
	while (is_enabled && needed > done) {
		Bitu left = (needed - done);
		left *= freq_add;
		left  = (left >> FREQ_SHIFT) + ((left & FREQ_MASK)!=0);
		handler(static_cast<uint16_t>(left));
	}
}

void MixerChannel::AddSilence()
{
	MIXER_LockAudioDevice();

	if (done < needed) {
		if(prev_sample[0] == 0 && prev_sample[1] == 0) {
			done = needed;
			//Make sure the next samples are zero when they get switched to prev
			next_sample[0] = 0;
			next_sample[1] = 0;
			//This should trigger an instant request for new samples
			freq_counter = FREQ_NEXT;
		} else {
			bool stereo = last_samples_were_stereo;
			//Position where to write the data
			Bitu mixpos = mixer.pos + done;
			while (done < needed) {
				// Maybe depend on sample rate. (the 4)
				if (prev_sample[0] > 4)       next_sample[0] = prev_sample[0] - 4;
				else if (prev_sample[0] < -4) next_sample[0] = prev_sample[0] + 4;
				else next_sample[0] = 0;
				if (prev_sample[1] > 4)       next_sample[1] = prev_sample[1] - 4;
				else if (prev_sample[1] < -4) next_sample[1] = prev_sample[1] + 4;
				else next_sample[1] = 0;

				mixpos &= MIXER_BUFMASK;

				mixer.work[mixpos][0] += static_cast<int32_t>(
				        prev_sample[0] * volmul[0]);
				mixer.work[mixpos][1] += static_cast<int32_t>(
				        (stereo ? prev_sample[1] : prev_sample[0]) *
				        volmul[1]);

				prev_sample[0] = next_sample[0];
				prev_sample[1] = next_sample[1];
				mixpos++;
				done++;
				freq_counter = FREQ_NEXT;
			} 
		}
	}
	last_samples_were_silence = true;
	offset[0] = offset[1] = 0;

	MIXER_UnlockAudioDevice();
}

// Floating-point conversion from unsigned 8-bit to signed 16-bit.
// This is only used to populate a lookup table that's 20-fold faster.
constexpr int16_t u8to16(const int u_val)
{
	assert(u_val >= 0 && u_val <= UINT8_MAX);
	const auto s_val = u_val - 128;
	if (s_val > 0) {
		constexpr auto scalar = INT16_MAX / 127.0;
		return static_cast<int16_t>(round(s_val * scalar));
	}
	return static_cast<int16_t>(s_val * 256);
}

// 8-bit to 16-bit lookup tables
static int16_t lut_u8to16[UINT8_MAX + 1] = {};
constexpr int16_t *lut_s8to16 = lut_u8to16 + 128;

constexpr void fill_8to16_lut()
{
	for (int i = 0; i <= UINT8_MAX; ++i)
		lut_u8to16[i] = u8to16(i);
}

// 4 seems to work . Disabled for now
#define MIXER_UPRAMP_STEPS 0
#define MIXER_UPRAMP_SAVE 512

template<class Type,bool stereo,bool signeddata,bool nativeorder>
void MixerChannel::AddSamples(Bitu len, const Type* data) {
	MIXER_LockAudioDevice();

	last_samples_were_stereo = stereo;

	//Position where to write the data
	Bitu mixpos = mixer.pos + done;
	//Position in the incoming data
	Bitu pos = 0;
	//Mix and data for the full length
	while (1) {
		//Does new data need to get read?
		while (freq_counter >= FREQ_NEXT) {
			//Would this overflow the source data, then it's time to leave
			if (pos >= len) {
				last_samples_were_silence = false;
#if MIXER_UPRAMP_STEPS > 0
				if (offset[0] || offset[1]) {
					//Should be safe to do, as the value inside offset is 16 bit while offset itself is at least 32 bit
					offset[0] = (offset[0]*(MIXER_UPRAMP_STEPS-1))/MIXER_UPRAMP_STEPS;
					offset[1] = (offset[1]*(MIXER_UPRAMP_STEPS-1))/MIXER_UPRAMP_STEPS;
					if (offset[0] < MIXER_UPRAMP_SAVE && offset[0] > -MIXER_UPRAMP_SAVE) offset[0] = 0;
					if (offset[1] < MIXER_UPRAMP_SAVE && offset[1] > -MIXER_UPRAMP_SAVE) offset[1] = 0;
				}
#endif
				MIXER_UnlockAudioDevice();
				return;
			}
			freq_counter -= FREQ_NEXT;

			prev_sample[0] = next_sample[0];
			if (stereo) {
				prev_sample[1] = next_sample[1];
			}

			if ( sizeof( Type) == 1) {
				// unsigned 8-bit
				if (!signeddata) {
					if (stereo) {
						next_sample[0] = lut_u8to16[data[pos * 2 + 0]];
						next_sample[1] = lut_u8to16[data[pos * 2 + 1]];
					} else {
						next_sample[0] = lut_u8to16[data[pos]];
					}
				}
				// signed 8-bit
				else {
					if (stereo) {
						next_sample[0] = lut_s8to16[data[pos * 2 + 0]];
						next_sample[1] = lut_s8to16[data[pos * 2 + 1]];
					} else {
						next_sample[0] = lut_s8to16[data[pos]];
					}
				}
			//16bit and 32bit both contain 16bit data internally
			} else  {
				if (signeddata) {
					if (stereo) {
						if (nativeorder) {
							next_sample[0]=data[pos*2+0];
							next_sample[1]=data[pos*2+1];
						} else {
							if ( sizeof( Type) == 2) {
								next_sample[0]=(Bit16s)host_readw((HostPt)&data[pos*2+0]);
								next_sample[1]=(Bit16s)host_readw((HostPt)&data[pos*2+1]);
							} else {
								next_sample[0]=(Bit32s)host_readd((HostPt)&data[pos*2+0]);
								next_sample[1]=(Bit32s)host_readd((HostPt)&data[pos*2+1]);
							}
						}
					} else {
						if (nativeorder) {
							next_sample[0] = data[pos];
						} else {
							if ( sizeof( Type) == 2) {
								next_sample[0]=(Bit16s)host_readw((HostPt)&data[pos]);
							} else {
								next_sample[0]=(Bit32s)host_readd((HostPt)&data[pos]);
							}
						}
					}
				} else {
					if (stereo) {
						if (nativeorder) {
							next_sample[0]=(Bits)data[pos*2+0]-32768;
							next_sample[1]=(Bits)data[pos*2+1]-32768;
						} else {
							if ( sizeof( Type) == 2) {
								next_sample[0]=(Bits)host_readw((HostPt)&data[pos*2+0])-32768;
								next_sample[1]=(Bits)host_readw((HostPt)&data[pos*2+1])-32768;
							} else {
								next_sample[0]=(Bits)host_readd((HostPt)&data[pos*2+0])-32768;
								next_sample[1]=(Bits)host_readd((HostPt)&data[pos*2+1])-32768;
							}
						}
					} else {
						if (nativeorder) {
							next_sample[0]=(Bits)data[pos]-32768;
						} else {
							if ( sizeof( Type) == 2) {
								next_sample[0]=(Bits)host_readw((HostPt)&data[pos])-32768;
							} else {
								next_sample[0]=(Bits)host_readd((HostPt)&data[pos])-32768;
							}
						}
					}
				}
			}
			//This sample has been handled now, increase position
			pos++;
#if MIXER_UPRAMP_STEPS > 0
			if (last_samples_were_silence && pos == 1) {
				offset[0] = next_sample[0] - prev_sample[0];
				if (stereo) offset[1] = next_sample[1] - prev_sample[1];
				//Don't bother with small steps.
				if (offset[0] < (MIXER_UPRAMP_SAVE*4) && offset[0] > (-MIXER_UPRAMP_SAVE*4)) offset[0] = 0;
				if (offset[1] < (MIXER_UPRAMP_SAVE*4) && offset[1] > (-MIXER_UPRAMP_SAVE*4)) offset[1] = 0;
			}

			if (offset[0] || offset[1]) {
				next_sample[0] = next_sample[0] - (offset[0]*(MIXER_UPRAMP_STEPS*static_cast<Bits>(len)-static_cast<Bits>(pos))) /( MIXER_UPRAMP_STEPS*static_cast<Bits>(len) );
				next_sample[1] = next_sample[1] - (offset[1]*(MIXER_UPRAMP_STEPS*static_cast<Bits>(len)-static_cast<Bits>(pos))) /( MIXER_UPRAMP_STEPS*static_cast<Bits>(len) );
			}
#endif
		}

		// Process initial samples through an expanding envelope to
		// prevent severe clicks and pops. Becomes a no-op when done.
		envelope.Process(stereo, interpolate, prev_sample, next_sample);

		// Apply the left and right channel mappers only on write[..]
		// assignments.  This ensures the channels are mapped only once
		//(avoiding double-swapping) and also minimizes the places where
		// we use our mapping variables as array indexes.
		// Note that volumes are independent of the channels mapping.
		const Bit8u left_map(channel_map[0]);
		const Bit8u right_map(channel_map[1]);

		//Where to write
		mixpos &= MIXER_BUFMASK;
		if (!interpolate) {
			mixer.work[mixpos][0] += static_cast<int32_t>(
			        prev_sample[left_map] * volmul[0]);
			mixer.work[mixpos][1] += static_cast<int32_t>(
			        (stereo ? prev_sample[right_map]
			                : prev_sample[left_map]) *
			        volmul[1]);
		} else {
			Bits diff_mul = freq_counter & FREQ_MASK;
			Bits sample = prev_sample[left_map] + (((next_sample[left_map] - prev_sample[left_map]) * diff_mul) >> FREQ_SHIFT);
			mixer.work[mixpos][0] += static_cast<int32_t>(sample *
			                                              volmul[0]);
			if (stereo) {
				sample = prev_sample[right_map] + (((next_sample[right_map] - prev_sample[right_map]) * diff_mul) >> FREQ_SHIFT);
			}
			mixer.work[mixpos][1] += static_cast<int32_t>(sample *
			                                              volmul[1]);
		}
		//Prepare for next sample
		freq_counter += freq_add;
		mixpos++;
		done++;
	}
	
	MIXER_UnlockAudioDevice();
}

void MixerChannel::AddStretched(Bitu len,Bit16s * data) {
	MIXER_LockAudioDevice();

	if (done >= needed) {
		LOG_MSG("Can't add, buffer full");
		MIXER_UnlockAudioDevice();
		return;
	}
	//Target samples this inputs gets stretched into
	Bitu outlen = needed - done;
	Bitu index = 0;
	Bitu index_add = (len << FREQ_SHIFT)/outlen;
	Bitu mixpos = mixer.pos + done;
	Bitu pos = 0;

	while (outlen--) {
		Bitu new_pos = index >> FREQ_SHIFT;
		if (pos != new_pos) {
			pos = new_pos;
			//Forward the previous sample
			prev_sample[0] = data[0];
			data++;
		}
		Bits diff = data[0] - prev_sample[0];
		Bits diff_mul = index & FREQ_MASK;
		index += index_add;
		mixpos &= MIXER_BUFMASK;
		Bits sample = prev_sample[0] + ((diff * diff_mul) >> FREQ_SHIFT);
		mixer.work[mixpos][0] += static_cast<int32_t>(sample * volmul[0]);
		mixer.work[mixpos][1] += static_cast<int32_t>(sample * volmul[1]);
		mixpos++;
	}

	done = needed;
	
	MIXER_UnlockAudioDevice();
}

void MixerChannel::AddSamples_m8(Bitu len, const Bit8u * data) {
	AddSamples<Bit8u,false,false,true>(len,data);
}
void MixerChannel::AddSamples_s8(Bitu len,const Bit8u * data) {
	AddSamples<Bit8u,true,false,true>(len,data);
}
void MixerChannel::AddSamples_m8s(Bitu len,const Bit8s * data) {
	AddSamples<Bit8s,false,true,true>(len,data);
}
void MixerChannel::AddSamples_s8s(Bitu len,const Bit8s * data) {
	AddSamples<Bit8s,true,true,true>(len,data);
}
void MixerChannel::AddSamples_m16(Bitu len,const Bit16s * data) {
	AddSamples<Bit16s,false,true,true>(len,data);
}
void MixerChannel::AddSamples_s16(Bitu len,const Bit16s * data) {
	AddSamples<Bit16s,true,true,true>(len,data);
}
void MixerChannel::AddSamples_m16u(Bitu len,const Bit16u * data) {
	AddSamples<Bit16u,false,false,true>(len,data);
}
void MixerChannel::AddSamples_s16u(Bitu len,const Bit16u * data) {
	AddSamples<Bit16u,true,false,true>(len,data);
}
void MixerChannel::AddSamples_m32(Bitu len,const Bit32s * data) {
	AddSamples<Bit32s,false,true,true>(len,data);
}
void MixerChannel::AddSamples_s32(Bitu len,const Bit32s * data) {
	AddSamples<Bit32s,true,true,true>(len,data);
}
void MixerChannel::AddSamples_m16_nonnative(Bitu len,const Bit16s * data) {
	AddSamples<Bit16s,false,true,false>(len,data);
}
void MixerChannel::AddSamples_s16_nonnative(Bitu len,const Bit16s * data) {
	AddSamples<Bit16s,true,true,false>(len,data);
}
void MixerChannel::AddSamples_m16u_nonnative(Bitu len,const Bit16u * data) {
	AddSamples<Bit16u,false,false,false>(len,data);
}
void MixerChannel::AddSamples_s16u_nonnative(Bitu len,const Bit16u * data) {
	AddSamples<Bit16u,true,false,false>(len,data);
}
void MixerChannel::AddSamples_m32_nonnative(Bitu len,const Bit32s * data) {
	AddSamples<Bit32s,false,true,false>(len,data);
}
void MixerChannel::AddSamples_s32_nonnative(Bitu len,const Bit32s * data) {
	AddSamples<Bit32s,true,true,false>(len,data);
}

void MixerChannel::FillUp()
{
	if (!is_enabled || done < mixer.done)
		return;
	const auto index = PIC_TickIndex();
	MIXER_LockAudioDevice();
	Mix((Bitu)(index * static_cast<double>(mixer.needed)));
	MIXER_UnlockAudioDevice();
}

extern bool ticksLocked;
static inline bool Mixer_irq_important()
{
	/* In some states correct timing of the irqs is more important than
	 * non stuttering audo */
	return (ticksLocked || (CaptureState & (CAPTURE_WAVE|CAPTURE_VIDEO)));
}

static constexpr Bit32u calc_tickadd(Bit32u freq) {
#if TICK_SHIFT > 16
	Bit64u freq64 = static_cast<Bit64u>(freq);
	freq64 = (freq64<<TICK_SHIFT)/1000;
	Bit32u r = static_cast<Bit32u>(freq64);
	return r;
#else
	return (freq<<TICK_SHIFT)/1000;
#endif
}

/* Mix a certain amount of new samples */
static void MIXER_MixData(Bitu needed) {
	MixerChannel * chan=mixer.channels;
	while (chan) {
		chan->Mix(needed);
		chan=chan->next;
	}
	if (CaptureState & (CAPTURE_WAVE|CAPTURE_VIDEO)) {
		int16_t convert[1024][2];
		const size_t added = std::min<size_t>(needed - mixer.done, 1024);
		size_t readpos = (mixer.pos + mixer.done) & MIXER_BUFMASK;
		for (size_t i = 0; i < added; i++) {
			const int32_t sample_1 = mixer.work[readpos][0] >> MIXER_VOLSHIFT;
			const int32_t sample_2 = mixer.work[readpos][1] >> MIXER_VOLSHIFT;
			const int16_t s1 = MIXER_CLIP(sample_1);
			const int16_t s2 = MIXER_CLIP(sample_2);
			convert[i][0] = host_to_le16(s1);
			convert[i][1] = host_to_le16(s2);
			readpos = (readpos + 1) & MIXER_BUFMASK;
		}
		CAPTURE_AddWave(mixer.freq, added, reinterpret_cast<int16_t*>(convert));
	}
	//Reset the the tick_add for constant speed
	if( Mixer_irq_important() )
		mixer.tick_add = calc_tickadd(mixer.freq);
	mixer.done = static_cast<uint32_t>(needed);
}

static void MIXER_Mix()
{
	MIXER_LockAudioDevice();
	MIXER_MixData(mixer.needed);
	mixer.tick_counter += mixer.tick_add;
	mixer.needed+=(mixer.tick_counter >> TICK_SHIFT);
	mixer.tick_counter &= TICK_MASK;
	MIXER_UnlockAudioDevice();
}

static void MIXER_Mix_NoSound()
{
	MIXER_MixData(mixer.needed);
	/* Clear piece we've just generated */
	for (Bitu i=0;i<mixer.needed;i++) {
		mixer.work[mixer.pos][0]=0;
		mixer.work[mixer.pos][1]=0;
		mixer.pos=(mixer.pos+1)&MIXER_BUFMASK;
	}
	/* Reduce count in channels */
	for (MixerChannel * chan=mixer.channels;chan;chan=chan->next) {
		if (chan->done>mixer.needed) chan->done-=mixer.needed;
		else chan->done=0;
	}
	/* Set values for next tick */
	mixer.tick_counter += mixer.tick_add;
	mixer.needed = (mixer.tick_counter >> TICK_SHIFT);
	mixer.tick_counter &= TICK_MASK;
	mixer.done=0;
}

#define INDEX_SHIFT_LOCAL 14

static void SDLCALL MIXER_CallBack([[maybe_unused]] void *userdata, Uint8 *stream, int len)
{
	memset(stream, 0, len);
	auto need = static_cast<uint32_t>(len / MIXER_SSIZE);
	Bit16s *output = (Bit16s *)stream;
	Bit32u reduce = 0;
	Bit32u pos = 0;
	// Local resampling counter to manipulate the data when sending it off
	// to the callback
	Bit32u index_add = (1 << INDEX_SHIFT_LOCAL);
	Bit32u index = (index_add % need) ? need : 0;

	Bits sample = 0;
	/* Enough room in the buffer ? */
	if (mixer.done < need) {
		//LOG_WARNING("Full underrun need %d, have %d, min %d", need, mixer.done, mixer.min_needed);
		if ((need - mixer.done) > (need >> 7)) // Max 1 percent stretch.
			return;
		reduce = mixer.done;
		index_add = (reduce << INDEX_SHIFT_LOCAL) / need;
		mixer.tick_add = calc_tickadd(mixer.freq + mixer.min_needed);
	} else if (mixer.done < mixer.max_needed) {
		auto left = mixer.done - need;
		if (left < mixer.min_needed) {
			if (!Mixer_irq_important()) {
				auto needed = mixer.needed - need;
				auto diff = (mixer.min_needed > needed ? mixer.min_needed.load()
				                                       : needed) - left;
				mixer.tick_add = calc_tickadd(mixer.freq +
				                              (diff * 3));
				left = 0; // No stretching as we compensate with
				          // the tick_add value
			} else {
				left = (mixer.min_needed - left);
				left = 1 + (2 * left) / mixer.min_needed; // left=1,2,3
			}
			//LOG_WARNING("needed underrun need %d, have %d, min %d, left %d", need, mixer.done, mixer.min_needed, left);
			reduce = need - left;
			index_add = (reduce << INDEX_SHIFT_LOCAL) / need;
		} else {
			reduce = need;
			index_add = (1 << INDEX_SHIFT_LOCAL);
			//			LOG_MSG("regular run need %d, have
			//%d, min %d, left %d", need, mixer.done,
			//mixer.min_needed, left);

			/* Mixer tick value being updated:
			 * 3 cases:
			 * 1) A lot too high. >division by 5. but maxed by 2*
			 * min to prevent too fast drops. 2) A little too high >
			 * division by 8 3) A little to nothing above the
			 * min_needed buffer > go to default value
			 */
			auto diff = left - mixer.min_needed;
			if (diff > (mixer.min_needed << 1))
				diff = mixer.min_needed << 1;
			if (diff > (mixer.min_needed >> 1))
				mixer.tick_add = calc_tickadd(mixer.freq -
				                              (diff / 5));
			else if (diff > (mixer.min_needed >> 2))
				mixer.tick_add = calc_tickadd(mixer.freq -
				                              (diff >> 3));
			else
				mixer.tick_add = calc_tickadd(mixer.freq);
		}
	} else {
		/* There is way too much data in the buffer */
		LOG_WARNING("overflow run need %u, have %u, min %u", need, mixer.done.load(), mixer.min_needed.load());
		if (mixer.done > MIXER_BUFSIZE)
			index_add = MIXER_BUFSIZE - 2 * mixer.min_needed;
		else
			index_add = mixer.done - 2 * mixer.min_needed;
		index_add = (index_add << INDEX_SHIFT_LOCAL) / need;
		reduce = mixer.done - 2 * mixer.min_needed;
		mixer.tick_add = calc_tickadd(mixer.freq - (mixer.min_needed / 5));
	}
	/* Reduce done count in all channels */
	for (MixerChannel * chan=mixer.channels;chan;chan=chan->next) {
		if (chan->done>reduce) chan->done-=reduce;
		else chan->done=0;
	}

	// Reset mixer.tick_add when irqs are important
	if( Mixer_irq_important() )
		mixer.tick_add = calc_tickadd(mixer.freq);

	mixer.done -= reduce;
	mixer.needed -= reduce;
	pos = mixer.pos;
	mixer.pos = (mixer.pos + reduce) & MIXER_BUFMASK;
	if (need != reduce) {
		while (need--) {
			const auto i = (pos + (index >> INDEX_SHIFT_LOCAL)) & MIXER_BUFMASK;
			index += index_add;
			sample = mixer.work[i][0] >> MIXER_VOLSHIFT;
			*output++ = MIXER_CLIP(sample);
			sample = mixer.work[i][1] >> MIXER_VOLSHIFT;
			*output++ = MIXER_CLIP(sample);
		}
		/* Clean the used buffer */
		while (reduce--) {
			pos &= MIXER_BUFMASK;
			mixer.work[pos][0] = 0;
			mixer.work[pos][1] = 0;
			pos++;
		}
	} else {
		while (reduce--) {
			pos &= MIXER_BUFMASK;
			sample = mixer.work[pos][0] >> MIXER_VOLSHIFT;
			*output++ = MIXER_CLIP(sample);
			sample = mixer.work[pos][1] >> MIXER_VOLSHIFT;
			*output++ = MIXER_CLIP(sample);
			mixer.work[pos][0] = 0;
			mixer.work[pos][1] = 0;
			pos++;
		}
	}
}

#undef INDEX_SHIFT_LOCAL

static void MIXER_Stop([[maybe_unused]] Section *sec)
{}

class MIXER final : public Program {
public:
	void MakeVolume(char * scan,float & vol0,float & vol1) {
		Bitu w=0;
		bool db=(toupper(*scan)=='D');
		if (db) scan++;
		while (*scan) {
			if (*scan==':') {
				++scan;w=1;
			}
			char * before=scan;
			float val=(float)strtod(scan,&scan);
			if (before==scan) {
				++scan;continue;
			}
			if (!db) val/=100;
			else
				val = powf(10.0f, val / 20.0f);
			if (val<0) val=1.0f;
			if (!w) {
				vol0=val;
			} else {
				vol1=val;
			}
		}
		if (!w) vol1=vol0;
	}

	void Run()
	{
		if(cmd->FindExist("/LISTMIDI")) {
			ListMidi();
			return;
		}
		if (cmd->FindString("MASTER",temp_line,false)) {
			MakeVolume((char *)temp_line.c_str(),mixer.mastervol[0],mixer.mastervol[1]);
		}
		MixerChannel * chan = mixer.channels;
		while (chan) {
			if (cmd->FindString(chan->name,temp_line,false)) {
				float left_vol = 0;
				float right_vol = 0;
				MakeVolume(&temp_line[0], left_vol, right_vol);
				chan->SetVolume(left_vol, right_vol);
			}
			chan->UpdateVolume();
			chan = chan->next;
		}
		if (cmd->FindExist("/NOSHOW")) return;
		WriteOut("Channel  Main    Main(dB)\n");
		ShowVolume("MASTER",mixer.mastervol[0],mixer.mastervol[1]);
		for (chan = mixer.channels;chan;chan = chan->next)
			ShowVolume(chan->name,chan->volmain[0],chan->volmain[1]);
	}

private:
	void ShowVolume(const char * name,float vol0,float vol1) {
		WriteOut("%-8s %3.0f:%-3.0f  %+3.2f:%-+3.2f \n", name,
		         static_cast<double>(vol0 * 100),
		         static_cast<double>(vol1 * 100),
		         static_cast<double>(20 * log(vol0) / log(10.0f)),
		         static_cast<double>(20 * log(vol1) / log(10.0f)));
	}

	void ListMidi() { MIDI_ListAll(this); }
};

static void MIXER_ProgramStart(Program * * make) {
	*make=new MIXER;
}

MixerChannel* MixerObject::Install(MIXER_Handler handler,Bitu freq,const char * name){
	if(!installed) {
		if(strlen(name) > 31) E_Exit("Too long mixer channel name");
		safe_strcpy(m_name, name);
		installed = true;
		return MIXER_AddChannel(handler,freq,name);
	} else {
		E_Exit("already added mixer channel.");
		return 0; //Compiler happy
	}
}

MixerObject::~MixerObject(){
	if(!installed) return;
	MIXER_DelChannel(MIXER_FindChannel(m_name));
}

void MIXER_Init(Section* sec) {
	sec->AddDestroyFunction(&MIXER_Stop);

	Section_prop * section=static_cast<Section_prop *>(sec);
	/* Read out config section */

	mixer.nosound=section->Get_bool("nosound");
	mixer.freq = static_cast<uint32_t>(section->Get_int("rate"));
	mixer.blocksize = static_cast<uint16_t>(section->Get_int("blocksize"));
	const auto negotiate = static_cast<bool>(section->Get_bool("negotiate"));

	/* Start the Mixer using SDL Sound at 22 khz */
	SDL_AudioSpec spec;
	SDL_AudioSpec obtained;

	spec.freq = static_cast<int>(mixer.freq);
	spec.format = AUDIO_S16SYS;
	spec.channels = 2;
	spec.callback = MIXER_CallBack;
	spec.userdata = nullptr;
	spec.samples = mixer.blocksize;

	int sdl_allow_flags = 0;

	if (negotiate) {
		sdl_allow_flags |= SDL_AUDIO_ALLOW_FREQUENCY_CHANGE;
#if defined(SDL_AUDIO_ALLOW_SAMPLES_CHANGE)
		sdl_allow_flags |= SDL_AUDIO_ALLOW_SAMPLES_CHANGE;
#endif
	}

	mixer.tick_counter=0;
	if (mixer.nosound) {
		LOG_MSG("MIXER: No Sound Mode Selected.");
		mixer.tick_add=calc_tickadd(mixer.freq);
		TIMER_AddTickHandler(MIXER_Mix_NoSound);
	} else if ((mixer.sdldevice = SDL_OpenAudioDevice(NULL, 0, &spec, &obtained, sdl_allow_flags)) ==0 ) {
		mixer.nosound = true;
		LOG_WARNING("MIXER: Can't open audio: %s , running in nosound mode.",SDL_GetError());
		mixer.tick_add=calc_tickadd(mixer.freq);
		TIMER_AddTickHandler(MIXER_Mix_NoSound);
	} else {
		// Does SDL want something other than stereo output?
		if (obtained.channels != spec.channels)
			E_Exit("SDL gave us %u-channel output but we require %u channels",
			       obtained.channels, spec.channels);

		// Does SDL want a different playback rate?
		assert(obtained.freq > 0 &&
		       static_cast<unsigned>(obtained.freq) < UINT32_MAX);
		const auto obtained_freq = static_cast<uint32_t>(obtained.freq);
		if (obtained_freq != mixer.freq) {
			LOG_WARNING("MIXER: SDL changed the playback rate from %u to %u Hz",
			        mixer.freq, obtained_freq);
			mixer.freq = obtained_freq;
		}

		// Does SDL want a different blocksize?
		const auto obtained_blocksize = obtained.samples;
		if (obtained_blocksize != mixer.blocksize) {
			LOG_WARNING("MIXER: SDL changed the blocksize from %u to %u frames",
			        mixer.blocksize, obtained_blocksize);
			mixer.blocksize = obtained_blocksize;
		}
		mixer.tick_add = calc_tickadd(mixer.freq);
		TIMER_AddTickHandler(MIXER_Mix);
		SDL_PauseAudioDevice(mixer.sdldevice, 0);

		LOG_MSG("MIXER: Negotiated %u-channel %u-Hz audio in %u-frame blocks",
		        obtained.channels, mixer.freq, mixer.blocksize);
	}

	//1000 = 8 *125
	mixer.tick_counter = (mixer.freq%125)?TICK_NEXT:0;
	const auto requested_prebuffer = section->Get_int("prebuffer");
	mixer.min_needed = static_cast<uint16_t>(clamp(requested_prebuffer, 0, 100));
	mixer.min_needed = (mixer.freq * mixer.min_needed) / 1000;
	mixer.max_needed = mixer.blocksize * 2 + 2 * mixer.min_needed;
	mixer.needed = mixer.min_needed + 1;

	// Initialize the 8-bit to 16-bit lookup table
	fill_8to16_lut();

	PROGRAMS_MakeFile("MIXER.COM",MIXER_ProgramStart);
}

void MIXER_CloseAudioDevice()
{
	if (!mixer.nosound) {
		if (mixer.sdldevice != 0) {
			SDL_CloseAudioDevice(mixer.sdldevice);
			mixer.sdldevice = 0;
		}
	}
}
