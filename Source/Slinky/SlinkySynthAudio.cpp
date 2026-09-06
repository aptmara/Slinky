#include "SlinkySynthAudio.h"
#include "Sound/SoundWaveProcedural.h"

namespace
{
	// Renders Spec into Samples starting at Offset, returning the next free offset - shared by
	// Render() (one call, offset 0) and RenderArpeggio() (one call per note).
	int32 RenderToneInto(TArray<int16>& Samples, int32 Offset, const FSlinkyToneSpec& Spec)
	{
		const int32 NumSamples = FMath::Max(1, FMath::RoundToInt(Spec.DurationSeconds * FSlinkySynthAudio::SampleRate));
		// Fixed seed: the same ESlinkySfx must sound identical every time it plays - exactly what a
		// frequently-repeated short UI/impact blip should do, not something that drifts play to play.
		FRandomStream NoiseStream(1337);
		double Phase = 0.0;
		const float AttackAlpha = Spec.DurationSeconds > 0.0f
			? FMath::Clamp(Spec.AttackSeconds / Spec.DurationSeconds, 0.0f, 1.0f)
			: 0.0f;

		for (int32 i = 0; i < NumSamples; ++i)
		{
			const float Alpha = NumSamples > 1 ? static_cast<float>(i) / (NumSamples - 1) : 1.0f;

			float Envelope;
			if (AttackAlpha > 0.0f && Alpha < AttackAlpha)
			{
				Envelope = Alpha / AttackAlpha;
			}
			else
			{
				const float DecayAlpha = AttackAlpha < 1.0f ? (Alpha - AttackAlpha) / (1.0f - AttackAlpha) : 1.0f;
				Envelope = FMath::Pow(1.0f - FMath::Clamp(DecayAlpha, 0.0f, 1.0f), Spec.DecayCurve);
			}

			const float Freq = FMath::Lerp(Spec.StartFrequencyHz, Spec.EndFrequencyHz, Alpha);
			Phase += 2.0 * PI * Freq / FSlinkySynthAudio::SampleRate;

			float Tone = FMath::Sin(static_cast<float>(Phase));
			if (Spec.HarmonicAmount > 0.0f)
			{
				Tone = FMath::Lerp(Tone, FMath::Sin(static_cast<float>(Phase * 2.0)), Spec.HarmonicAmount);
			}

			const float Noise = NoiseStream.FRandRange(-1.0f, 1.0f);
			const float Signal = FMath::Lerp(Tone, Noise, Spec.NoiseAmount) * Envelope * Spec.Volume;

			Samples[Offset + i] = static_cast<int16>(FMath::Clamp(Signal, -1.0f, 1.0f) * 32000.0f);
		}

		return Offset + NumSamples;
	}
}

TArray<int16> FSlinkySynthAudio::Render(const FSlinkyToneSpec& Spec)
{
	const int32 NumSamples = FMath::Max(1, FMath::RoundToInt(Spec.DurationSeconds * SampleRate));
	TArray<int16> Samples;
	Samples.SetNumUninitialized(NumSamples);
	RenderToneInto(Samples, 0, Spec);
	return Samples;
}

TArray<int16> FSlinkySynthAudio::RenderArpeggio(const TArray<float>& FrequenciesHz, float NoteDurationSeconds, float Volume)
{
	const int32 SamplesPerNote = FMath::Max(1, FMath::RoundToInt(NoteDurationSeconds * SampleRate));
	TArray<int16> Samples;
	Samples.SetNumUninitialized(SamplesPerNote * FMath::Max(FrequenciesHz.Num(), 1));

	int32 Offset = 0;
	for (float Freq : FrequenciesHz)
	{
		FSlinkyToneSpec NoteSpec;
		NoteSpec.StartFrequencyHz = Freq;
		NoteSpec.EndFrequencyHz = Freq;
		NoteSpec.DurationSeconds = NoteDurationSeconds;
		NoteSpec.AttackSeconds = NoteDurationSeconds * 0.08f;
		NoteSpec.DecayCurve = 2.2f;
		NoteSpec.HarmonicAmount = 0.3f;
		NoteSpec.Volume = Volume;
		Offset = RenderToneInto(Samples, Offset, NoteSpec);
	}

	return Samples;
}

TArray<int16> FSlinkySynthAudio::RenderMetallicRing(float BaseFrequencyHz, float DurationSeconds, float Volume)
{
	// Inharmonic partial ratios (not 1, 2, 3, ...) and relative amplitudes/decay rates - loosely
	// modeled on a struck metal bar: the higher, quieter partials die out faster than the
	// fundamental, which is what makes the whole thing shorten and darken into a low hum instead of
	// staying bright throughout - a single-partial sine sweep (Render()) can't produce that.
	struct FPartial { float Ratio; float Amplitude; float DecayRate; };
	static const FPartial Partials[] = {
		{ 1.00f, 1.00f, 3.0f },
		{ 2.41f, 0.55f, 6.0f },
		{ 3.86f, 0.35f, 9.0f },
		{ 5.43f, 0.20f, 13.0f },
	};

	const int32 NumSamples = FMath::Max(1, FMath::RoundToInt(DurationSeconds * SampleRate));
	TArray<int16> Samples;
	Samples.SetNumUninitialized(NumSamples);

	const float AttackSeconds = FMath::Min(0.006f, DurationSeconds * 0.1f);
	for (int32 i = 0; i < NumSamples; ++i)
	{
		const float Time = static_cast<float>(i) / SampleRate;
		const float Attack = AttackSeconds > 0.0f ? FMath::Clamp(Time / AttackSeconds, 0.0f, 1.0f) : 1.0f;

		float Signal = 0.0f;
		for (const FPartial& Partial : Partials)
		{
			const float Decay = FMath::Exp(-Partial.DecayRate * Time);
			Signal += FMath::Sin(2.0f * PI * BaseFrequencyHz * Partial.Ratio * Time) * Partial.Amplitude * Decay;
		}
		Signal *= Attack * Volume / 2.0f;

		Samples[i] = static_cast<int16>(FMath::Clamp(Signal, -1.0f, 1.0f) * 32000.0f);
	}

	return Samples;
}

USoundWaveProcedural* FSlinkySynthAudio::BuildPlayableWave(UObject* Outer, const TArray<int16>& Samples)
{
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(Outer);
	Wave->SetSampleRate(SampleRate);
	Wave->NumChannels = 1;
	Wave->SetNumFrames(Samples.Num());
	Wave->Duration = static_cast<float>(Samples.Num()) / SampleRate;
	Wave->SoundGroup = SOUNDGROUP_UI;
	Wave->bLooping = false;
	// Queued once, up front, rather than fed incrementally - this isn't a real streaming source,
	// just a short precomputed clip handed to the audio engine's procedural-wave pull model.
	Wave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
	return Wave;
}
