#pragma once

#include "CoreMinimal.h"

class USoundWaveProcedural;

// Every distinct code-synthesized sound effect the game plays - see
// USlinkyGameInstance::PlaySfx/GetOrBuildSfx for where each is triggered and how it's rendered.
// StepMetal/StepPlastic and ResetMetal/ResetPlastic are picked between at the call site by
// ASlinkyActor::GetCoilMaterialStyle() - see RegisterGroundContact()/ResetSlinky().
enum class ESlinkySfx : uint8
{
	StepMetal,
	StepPlastic,
	ComboTierUp,
	ButtonClick,
	PauseOpen,
	PauseClose,
	Grab,
	Release,
	ResetMetal,
	ResetPlastic,
	TimeWarning,
	TimeUp,
};

// One short procedurally-synthesized clip's shape - built entirely from math (a sine oscillator,
// a linear-attack/curved-decay envelope, and an optional noise/harmonic blend), not an imported
// audio asset - matching this project's "everything code-built" approach elsewhere (WidgetTree-only
// UI, procedural materials/geometry). See FSlinkySynthAudio::Render.
struct FSlinkyToneSpec
{
	// Frequency sweeps linearly from Start to End across the whole clip - equal values give a
	// flat tone, a falling/rising End gives a chirp/swoop.
	float StartFrequencyHz = 440.0f;
	float EndFrequencyHz = 440.0f;
	float DurationSeconds = 0.15f;
	// Linear ramp-up from silence, in seconds; the rest of the clip decays from there.
	float AttackSeconds = 0.005f;
	// Decay shape after the attack: 1 = linear fade-out, higher = snappier/more percussive.
	float DecayCurve = 3.0f;
	// 0 = pure tone, 1 = pure white noise, blended in between - used for click/impact sounds.
	float NoiseAmount = 0.0f;
	// Blends in a quiet one-octave-up overtone for a slightly richer, less pure-sine timbre.
	float HarmonicAmount = 0.25f;
	float Volume = 0.5f;
};

// Renders short (well under a second) UI/gameplay SFX at runtime from pure math and hands them
// back as a ready-to-play USoundWaveProcedural - see USlinkyGameInstance::PlaySfx.
class FSlinkySynthAudio
{
public:
	static constexpr int32 SampleRate = 22050;

	// Pure data, no UObject involved - USlinkyGameInstance caches the result per ESlinkySfx so the
	// math only runs once, not on every play.
	static TArray<int16> Render(const FSlinkyToneSpec& Spec);

	// A short sequence of discrete notes (each its own brief attack/decay envelope) concatenated
	// into one clip - used for the combo-tier-up chime, where a single swept tone would read as a
	// siren rather than a musical flourish.
	static TArray<int16> RenderArpeggio(const TArray<float>& FrequenciesHz, float NoteDurationSeconds, float Volume);

	// Sums several sine partials at inharmonic (non-integer) frequency ratios, each with its own
	// slow decay - real metal doesn't ring at clean octave/fifth ratios the way a plucked string
	// does, which is exactly what separates this from Render()'s single-partial-plus-one-harmonic
	// tone. Used for the coil's metal-style step/reset clangs (see ESlinkySfx::StepMetal/ResetMetal).
	static TArray<int16> RenderMetallicRing(float BaseFrequencyHz, float DurationSeconds, float Volume);

	// Wraps already-rendered PCM samples in a fresh USoundWaveProcedural ready to hand to
	// UGameplayStatics::SpawnSound2D. A new instance every call (rather than one long-lived object
	// reused across plays) so two overlapping plays of the same effect - e.g. two steps landing
	// close together - never fight over one internal audio queue.
	static USoundWaveProcedural* BuildPlayableWave(UObject* Outer, const TArray<int16>& Samples);
};
