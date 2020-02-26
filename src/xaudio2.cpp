/* Windows xAudio2 API Output.
 *
 * Copyright (C) 2016 Reece H. Dunn
 *
 * This file is part of pcaudiolib.
 *
 * pcaudiolib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * pcaudiolib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with pcaudiolib.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "audio_priv.h"

#ifndef HAVE_FAUDIO_H

// NOTE: XAudio2.h fails to build with a C compiler
#include <XAudio2.h>
#pragma comment(lib, "xaudio2.lib")

#else

#include <FAudio.h>

// Wrappers for XAudio2 interfaces
// to minimize changes in implementation code.

typedef FAudio IXAudio2;
typedef FAudioMasteringVoice IXAudio2MasteringVoice;
typedef FAudioSourceVoice IXAudio2SourceVoice;
typedef FAudioVoiceState XAUDIO2_VOICE_STATE;
typedef FAudioBuffer XAUDIO2_BUFFER;
#define WAVE_FORMAT_IEEE_FLOAT FAUDIO_FORMAT_IEEE_FLOAT

#endif

struct xaudio2_object
{
	struct audio_object vtable;
	IXAudio2 *audio;
	IXAudio2MasteringVoice *mastering;
	IXAudio2SourceVoice *source;
	WAVEFORMATEX *format;
	LPWSTR devicename;
};

void
xaudio2_object_close(struct audio_object *object);

#define to_xaudio2_object(object) container_of(object, struct xaudio2_object, vtable)

int
xaudio2_object_open(struct audio_object *object,
                    enum audio_object_format format,
                    uint32_t rate,
                    uint8_t channels)
{
	struct xaudio2_object *self = to_xaudio2_object(object);
	if (self->mastering != NULL)
		return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);

	HRESULT hr;
#	ifndef HAVE_FAUDIO_H
	hr = self->audio->CreateMasteringVoice(&self->mastering);
#	else
	hr = FAudio_CreateMasteringVoice(self->audio, &self->mastering,
		FAUDIO_DEFAULT_CHANNELS, FAUDIO_DEFAULT_SAMPLERATE, 0u, 0u, nullptr);
#	endif
	if (FAILED(hr))
		goto error;

	hr = CreateWaveFormat(format, rate, channels, &self->format);
	if (FAILED(hr))
		goto error;

#	ifndef HAVE_FAUDIO_H
 	hr = self->audio->CreateSourceVoice(&self->source, self->format);
#	else
	hr = FAudio_CreateSourceVoice(self->audio, &self->source, self->format,
		0u, FAUDIO_DEFAULT_FREQ_RATIO, nullptr, nullptr, nullptr);
#	endif
	if (FAILED(hr))
		goto error;

	return S_OK;
error:
	xaudio2_object_close(object);
	return hr;
}

void
xaudio2_object_close(struct audio_object *object)
{
	struct xaudio2_object *self = to_xaudio2_object(object);

	if (self->source != NULL)
	{
#		ifndef HAVE_FAUDIO_H
		self->source->DestroyVoice();
#		else
		FAudioVoice_DestroyVoice(self->source);
#		endif
		self->source = NULL;
	}

	if (self->format != NULL)
	{
		CoTaskMemFree(self->format);
		self->format = NULL;
	}

	if (self->mastering != NULL)
	{
#		ifndef HAVE_FAUDIO_H
		self->mastering->DestroyVoice();
#		else
		FAudioVoice_DestroyVoice(self->mastering);
#		endif
		self->mastering = NULL;
	}
}

void
xaudio2_object_destroy(struct audio_object *object)
{
	struct xaudio2_object *self = to_xaudio2_object(object);

#	ifndef HAVE_FAUDIO_H
	self->audio->Release();
#	else
	FAudio_Release(self->audio);
#	endif
	free(self->devicename);
	free(self);

	CoUninitialize();
}

int
xaudio2_object_drain(struct audio_object *object)
{
	struct xaudio2_object *self = to_xaudio2_object(object);

	return S_OK;
}

int
xaudio2_object_flush(struct audio_object *object)
{
	struct xaudio2_object *self = to_xaudio2_object(object);

	return S_OK;
}

int
xaudio2_object_write(struct audio_object *object,
                     const void *data,
                     size_t bytes)
{
	struct xaudio2_object *self = to_xaudio2_object(object);

	XAUDIO2_BUFFER buffer = {0};
	buffer.AudioBytes = bytes;
	buffer.pAudioData = (const BYTE *)data;

	HRESULT hr = S_OK;
	if (SUCCEEDED(hr))
#		ifndef HAVE_FAUDIO_H
		hr = self->source->SubmitSourceBuffer(&buffer);
#		else
		hr = FAudioSourceVoice_SubmitSourceBuffer(self->source, &buffer,
			nullptr);
#		endif

	if (SUCCEEDED(hr))
#		ifndef HAVE_FAUDIO_H
		hr = self->source->Start(0);
#		else
		hr = FAudioSourceVoice_Start(self->source, 0, FAUDIO_COMMIT_NOW);
#		endif

	if (SUCCEEDED(hr)) while (true)
	{
		Sleep(10);

		XAUDIO2_VOICE_STATE state = { 0 };
#		ifndef HAVE_FAUDIO_H
		self->source->GetState(&state);
#		else
		FAudioSourceVoice_GetState(self->source, &state, 0u);
#		endif
		if (state.pCurrentBufferContext == NULL && state.BuffersQueued == 0)
			return hr;
	}

	return hr;
}

struct audio_object *
create_xaudio2_object(const char *device,
                      const char *application_name,
                      const char *description)
{
	CoInitialize(NULL);

	IXAudio2 *audio = NULL;
#	ifndef HAVE_FAUDIO_H
	HRESULT hr = XAudio2Create(&audio, 0, XAUDIO2_DEFAULT_PROCESSOR);
#	else
	HRESULT hr = FAudioCreate(&audio, 0, FAUDIO_DEFAULT_PROCESSOR);
#	endif
	if (FAILED(hr) || audio == NULL) {
		if (audio != NULL)
#			ifndef HAVE_FAUDIO_H
			audio->Release();
#			else
			FAudio_Release(audio);
#			endif

		CoUninitialize();
		return NULL;
	}

	struct xaudio2_object *self = (struct xaudio2_object *)malloc(sizeof(struct xaudio2_object));
	if (!self)
		return NULL;

	self->audio = audio;
	self->mastering = NULL;
	self->source = NULL;
	self->format = NULL;
	self->devicename = device ? str2wcs(device) : NULL;

	self->vtable.open = xaudio2_object_open;
	self->vtable.close = xaudio2_object_close;
	self->vtable.destroy = xaudio2_object_destroy;
	self->vtable.write = xaudio2_object_write;
	self->vtable.drain = xaudio2_object_drain;
	self->vtable.flush = xaudio2_object_flush;
	self->vtable.strerror = windows_hresult_strerror;

	return &self->vtable;
}
