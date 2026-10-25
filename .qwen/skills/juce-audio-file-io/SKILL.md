---
name: juce-audio-file-io
description: Read and write audio files through JUCE's AudioFormat / AudioFormatReader / AudioFormatWriter stack — ownership of streams, the options-object writer API, buffering and memory-mapped readers, codec capability flags — without leaking streams, misreading bit-depth, or touching the audio thread.
source: learned
---

# Reading and writing audio files (JUCE `juce_audio_formats`)

Distilled from the vendored `juce/` submodule (module version `9.0.4`):
`juce_audio_formats/juce_audio_formats.h` (config flags),
`format/juce_AudioFormat.h`, `format/juce_AudioFormatReader.{h,cpp}`,
`format/juce_AudioFormatWriter.{h,cpp}`, `format/juce_AudioFormatWriterOptions.h`,
`format/juce_AudioFormatManager.{h,cpp}`, `format/juce_AudioFormatReaderSource.h`,
`format/juce_AudioSubsectionReader.h`, `format/juce_BufferingAudioFormatReader.h`,
`format/juce_MemoryMappedAudioFormatReader.h`, `sampler/juce_Sampler.h`,
`codecs/juce_{Wav,Aiff,Flac,OggVorbis,Opus,MP3,LAMEEncoder,CoreAudio,WindowsMedia}AudioFormat.h`.

Module depends only on `juce_audio_basics`; nothing here needs an `AudioProcessor`.

## When to Use

- Loading a file into memory (an IR, a sample, a preview clip) or rendering audio out to disk.
- Format sniffing / opening an arbitrary user file: picking a reader from extension or magic bytes.
- Writing a WAV/AIFF/FLAC/Opus/Vorbis file with a specific bit-depth, channel layout, or metadata.
- Wiring a file into the `AudioSource` graph (`AudioFormatReaderSource`, `BufferingAudioSource`, `AudioTransportSource`).
- Random-access playback of long files: memory-mapped readers, subsection readers.
- Debugging: a reader that opens but returns silence, garbage at 24-bit, a leaked/asserting
  `InputStream`, `createWriterFor` returning `nullptr`, a file that plays in other apps but not yours,
  MP3 export doing nothing, `.aif` written at 32-bit float failing.

## 1. Know the four roles

| Class | Job | Who owns what |
|---|---|---|
| `AudioFormat` | descriptor + factory (name, extensions, capabilities, make a reader/writer) | stateless; the manager owns instances |
| `AudioFormatReader` | random-access sample reads + metadata | **owns and deletes its `InputStream`** (`~AudioFormatReader` → `delete input`) |
| `AudioFormatWriter` | sequential writes | **owns and deletes its `OutputStream`** |
| `AudioFormatManager` | list of formats + sniffing | `registerFormat()` takes ownership of the `AudioFormat*` |

Readers/writers are created by a format, never constructed directly (constructors are `protected`).

## 2. Opening a file

```cpp
AudioFormatManager manager;
manager.registerBasicFormats();                       // WAV (default) + AIFF + enabled codecs

if (auto reader = std::unique_ptr<AudioFormatReader>(manager.createReaderFor (file)))
{
    // reader->sampleRate (double), bitsPerSample (unsigned), lengthInSamples (int64),
    // numChannels (unsigned), usesFloatingPointData (bool), metadataValues, getChannelLayout()
}
```

- **`registerBasicFormats()` is not automatic.** An empty manager trips
  `jassert (getNumKnownFormats() > 0)` in `createReaderFor` and returns `nullptr` in release.
- Per-format instead of the manager: `WavAudioFormat fmt; fmt.createReaderFor (stream, deleteStreamIfOpeningFails)`
  — pass the raw `InputStream*` you just created; on success the reader owns it, and the bool decides
  whether it is deleted on *failure*. Forgetting this leaks one stream per failed attempt.
- Stream overload `createReaderFor (std::unique_ptr<InputStream>)` tries **every** registered format,
  seeking back to the original position between attempts. The stream must be seekable — a
  non-seekable stream trips `jassert (getPosition() == originalStreamPos)`. Wrap non-seekable sources
  in a `MemoryInputStream` or spool to disk first.
- Registration order is sniffing order; registering two formats with the same `getFormatName()`
  trips a debug assert.
- `getWildcardForAllFormats()` gives `"*.wav;*.aif;*.aiff;…"` for a `FileChooser`.

## 3. Reading samples — pick the right overload

```cpp
// A. float channels (most common)
float* chans[2] = { left.getData(), right.getData() };
reader->read (chans, 2, /*startSampleInSource*/ 0, /*numSamplesToRead*/ 512);

// B. AudioBuffer, with explicit mono/stereo source-channel mapping
reader->read (&buffer, /*startSampleInDestBuffer*/ 0, /*numSamples*/ n,
              /*readerStartSample*/ pos, /*useReaderLeftChan*/ true, /*useReaderRightChan*/ false);

// C. raw int channels + mono-duplication control
reader->read (intChans, nChans, start, n, /*fillLeftoverChannelsWithCopies*/ true);
```

Hard rules from `juce_AudioFormatReader.cpp`:

1. **Destination buffers must be 32-bit.** The `float* const*` overload `reinterpret_cast`s them to
   `int* const*` and converts in place with `1.0f / 0x7fffffff`. A `double` buffer is silently garbage.
2. `int* const*` semantics: fixed-point source → int32 full range `-0x80000000..0x7fffffff`
   **regardless of source bit-depth** (8/16/24 all left-shifted); floating-point source → already
   IEEE float, cast back to `(float**)`. Only `read(float* const*)` normalises for you, and only
   when `usesFloatingPointData == false`.
3. **Out-of-range reads are not errors.** Negative `startSampleInSource` and reads past
   `lengthInSamples` are zero-filled and return `true`. Check the return value for real I/O failure
   only; clamp ranges yourself when you need exact boundaries.
4. Channel-count mismatch is handled, not rejected: extra destination channels are zeroed, or
   duplicated from the last real channel when `fillLeftoverChannelsWithCopies` is true. Mono → stereo
   destination duplicates. Destination channel pointers may be `nullptr` to skip a channel.
5. `readSamples()` is the subclass virtual with *raw, unchecked* contract (`startSampleInFile >= 0`,
   `numDestChannels <= numChannels`). Callers always go through `read()`.

Scanning helpers (both allocate a 4096-sample temp buffer internally): `readMaxLevels()` for per-channel
min/max, and `searchForLevel(start, count, minMag, maxMag, minimumConsecutiveSamples)` for trim/loop
points — negative `count` scans backwards, `minimumConsecutiveSamples > 0` rejects spikes and returns
`-1` when nothing matches.

## 4. Writing a file — use the options object

```cpp
auto out = std::make_unique<FileOutputStream> (file);      // or MemoryOutputStream

auto opts = AudioFormatWriterOptions{}                      // alias: AudioFormatWriter::Options
              .withSampleRate (sr)
              .withChannelLayout (AudioChannelSet::stereo()) // supersedes withNumChannels()
              .withBitsPerSample (24)
              .withSampleFormat (AudioFormatWriterOptions::SampleFormat::automatic)
              .withMetadata (WavAudioFormat::riffInfoTitle, "Take 3")
              .withQualityOptionIndex (0);

std::unique_ptr<OutputStream> stream = std::move (out);
auto writer = fmt.createWriterFor (stream, opts);           // fmt = AudioFormat&
// success → `stream` is now nullptr (writer owns it); failure → stream intact, reusable
if (writer == nullptr)
    tryAnotherFormat (std::move (stream));
```

- `AudioFormatWriterOptions` is an **immutable builder**: every `withX()` returns a copy. Defaults:
  48000 Hz, 1 channel, `nullopt` layout, 16-bit, quality index 0, `SampleFormat::automatic`.
  `SampleFormat::integral` / `floatingPoint` only affect **WAV at 32-bit**; other formats ignore it.
- The two raw-`OutputStream*` `createWriterFor` overloads are `[[deprecated]]`. On failure the raw
  overload does **not** delete the stream — you must, or leak it.
- Validate against the format first: `getPossibleSampleRates()`, `getPossibleBitDepths()`,
  `canDoMono()/canDoStereo()`, `isChannelLayoutSupported(set)`, `getQualityOptions()`. Mismatch →
  `nullptr`, no diagnostic.
- Quality index selects an entry of `getQualityOptions()`: FLAC = compression level
  `"0 (Fastest)" … "8 (Highest quality)"`, Vorbis/Opus = bitrate labels (`"128 kbps"` …).
  PCM formats return an empty array — pass 0.

Writing:

```cpp
writer->writeFromFloatArrays (channelData, numChans, numSamples);   // float source
writer->writeFromAudioSampleBuffer (buf, start, n);
writer->writeFromAudioReader (otherReader, start, /*<0 = whole length*/ -1);  // 16384-sample chunks
writer->writeFromAudioSource (src, total, samplesPerBlock);         // src must already be prepareToPlay()'d
```

No sample-rate conversion anywhere in the writer path — resample before you write.
`write(const int**, n)` takes a **zero-terminated array of arrays**; a null entry writes silence for
that channel.

## 5. Writing from a live stream (`ThreadedWriter`)

```cpp
TimeSliceThread diskThread ("disk");
diskThread.startThread();                              // must be running, and outlive the writer

auto threaded = std::make_unique<AudioFormatWriter::ThreadedWriter>(
                    writer.release(),                  // ThreadedWriter takes ownership
                    diskThread, /*numSamplesToBuffer*/ 48000 * 10);

threaded->write (floatChannelData, numSamples);        // FIFO push; false = FIFO full
threaded.reset();                                      // destructor stops and flushes to disk
```

- `ThreadedWriter` **takes ownership of the `AudioFormatWriter`** and deletes it; `writer.release()` at the call.
- Deleting the `ThreadedWriter` is the *only* flush-and-close path; the `TimeSliceThread` must outlive it.
- `write()` takes `float* const*` (not `int**`), and none of the channels may be null.
- `setFlushInterval (n)` (default 0 = never) periodically calls `AudioFormatWriter::flush()` so a crash
  leaves a readable file. `flush()` must be called from the same thread that calls `write()`.
- `setDataReceiver()` lets a second consumer observe incoming blocks (e.g. drawing a waveform while
  recording); pass `nullptr` to detach, and the receiver must outlive the writer.

## 6. Adapters around a reader

| Class | Use for | Contract |
|---|---|---|
| `AudioFormatReaderSource` | feeding an `AudioSource` chain | `PositionableAudioSource`; owns reader via `OptionalScopedPointer`; `setLooping()` |
| `AudioSubsectionReader` | exposing a window of another reader (sample regions, loop edits) | samples outside the window read as 0; sample 0 here = `subsectionStartSample` there |
| `BufferingAudioReader` | smooth disk access for scrubbing/playback | wraps reader, prefetches on a `TimeSliceThread&`; takes ownership of the source; `setReadTimeout(ms)`, default **0** (a negative value waits forever) |
| `MemoryMappedAudioFormatReader` | fast random access on long files | **must call `mapEntireFile()` or `mapSectionOfFile()` before any read**; `getSample(i, out)` needs a result array of `numChannels`; `touchSample()` forces paging |
| `SamplerSound` + `SamplerVoice` | playing samples through `Synthesiser` | loads the whole stream into RAM in the constructor — the source reader is safe to delete as soon as the constructor returns; `getAudioData()` may return `nullptr` on failure |

Only WAV and AIFF override `createMemoryMappedReader()`; every other format returns `nullptr` —
always branch on the result.

## 7. Codec capability matrix

| Format | Read | Write | Bit depths | Notes |
|---|---|---|---|---|
| `WavAudioFormat` | ✔ | ✔ | 8, 16, 24, 32 | 8k–384k rates; BWAV/acid/INFO/ASWG metadata; `replaceMetadataInFile()` rewrites the metadata chunk in place; memory-mappable |
| `AiffAudioFormat` | ✔ | ✔ | 8, 16, 24 | **no 32-bit** — no 32-bit float AIFF here; memory-mappable |
| `FlacAudioFormat` | ✔ | ✔ | 16, 24 | lossless; quality = compression level 0–8 |
| `OggVorbisAudioFormat` | ✔ | ✔ | — | quality = bitrate labels; `id3title`/`id3artist`/… metadata keys |
| `OpusAudioFormat` | ✔ | ✔ | — | **always decodes at 48000 Hz**; the writer's sample rate describes *incoming* audio and is resampled to 48k |
| `MP3AudioFormat` | ✔ | ✘ | — | decoder only: `createWriterFor` is `jassertfalse; return nullptr` |
| `LAMEEncoderAudioFormat` | ✘ | ✔ | — | **off by default**; write-only, needs a path to a LAME *executable*, spools a temp WAV and shells out on writer deletion |
| `CoreAudioFormat` | ✔ | ✔ | — | macOS/iOS only (Apple frameworks) |
| `WindowsMediaAudioFormat` | ✔ | ✔ | — | Windows only |

Config flags (`juce_audio_formats.h`): `JUCE_USE_FLAC`/`JUCE_INCLUDE_FLAC_CODE`,
`JUCE_USE_OGGVORBIS`, `JUCE_USE_OPUS`/`JUCE_INCLUDE_OPUS_CODE`, `JUCE_USE_MP3AUDIOFORMAT` (all default 1),
`JUCE_USE_LAME_AUDIO_FORMAT` (default **0**), `JUCE_USE_WINDOWS_MEDIA_FORMAT` (forced to 0 off Windows).
Embedded libogg is compiled when *either* the Opus or Vorbis code is embedded — if you also link an
external libogg, that pair collides at link time. `JUCE_FLAC_INCLUDE_PATH` redirects to a system flac.

## 8. Metadata

Keys are plain `const char*` constants on the format class, carried in
`AudioFormatReader::metadataValues` / `AudioFormatWriterOptions::withMetadata()`. An unsupported key is
silently ignored by the writer — never assume a round trip.

- WAV/BWAV: `bwavDescription`, `bwavOriginator`, `bwavOriginatorRef`, `bwavOriginationDate`
  (`yyyy-mm-dd`), `bwavOriginationTime` (`hh-mm-ss`), `bwavCodingHistory`, `bwavTimeReference`;
  build them with `WavAudioFormat::createBWAVMetadata(...)` rather than formatting by hand.
- WAV INFO: ~70 `riffInfo*` keys; `internationalStandardRecordingCode` — **not** the deprecated `ISRC`
  constant, which is the same string as `riffInfoSource` and cannot be distinguished on read.
- Vorbis/Opus: `encoderName` (`"encoder"`), `id3title`, `id3artist`, `id3album`, `id3comment`,
  `id3date`, `id3genre`, `id3trackNumber`.
- `withMetadataValues(map)` **overwrites** all metadata; `withMetadata(k, v)` accumulates.

## 9. Threading

Nothing in this module is audio-thread safe: readers/writers allocate and do file I/O, and
`readMaxLevels`/`writeFromAudioReader` allocate temp buffers on every call. Load in `prepareToPlay`
(the non-callback setup path) or on a message/background thread, and stream recorded data through
`ThreadedWriter` rather than writing from the callback. `AudioSubsectionReader` and
`MemoryMappedAudioFormatReader` add no thread safety of their own — confine them to one thread.

## Pitfalls

- **Assuming `read()` failed when it returned silence.** Past EOF and negative offsets are zero-filled
  and `true`; only actual I/O problems return `false`.
- **Reading into `double` buffers** (e.g. a `std::vector<double>`) through `read(float* const*)` —
  the overload reinterprets the memory as `int*` and converts in place. 32-bit buffers only.
- **Manual `1/32768` scaling after `read(int**)`** — values are already normalised to the int32 range;
  the correct factor is `1/0x7fffffff`, and `read(float* const*)` applies it for you.
- **Reading the bit-depth literally.** `bitsPerSample` describes the *file*; the read API hands you
  32-bit containers for every fixed-point depth. Check `usesFloatingPointData`, not `bitsPerSample`,
  to decide whether conversion is needed.
- **Double-free or leak of the stream.** Reader deletes its `InputStream`; writer deletes its
  `OutputStream`. The only case where *you* still own the stream after a call is a **failed**
  `createWriterFor` (options API leaves the `unique_ptr` intact; raw-pointer API leaves a dangling raw
  pointer it never deletes).
- **Trying to write MP3 with `MP3AudioFormat`** — decode-only; a debug assert fires and you get
  `nullptr`. Enable `JUCE_USE_LAME_AUDIO_FORMAT` and supply a LAME executable, or export another format.
- **Forgetting `JUCE_USE_LAME_AUDIO_FORMAT=1`** (it is 0 by default) and getting an empty
  `LAMEEncoderAudioFormat` from the build, or assuming it can read MP3s — it cannot.
- **32-bit `.aif`** — AIFF here caps at 24-bit; a 32-bit request returns `nullptr` from `createWriterFor`.
  Use WAV 32-bit float (`withSampleFormat (floatingPoint)`).
- **Expecting an Opus file at the sample rate you passed in** — decoded audio is always 48 kHz.
- **Reading from a `MemoryMappedAudioFormatReader` before mapping** — call `mapEntireFile()` /
  `mapSectionOfFile()` first; the header says so and the failure is a crash, not an error code.
- **Deleting the `TimeSliceThread` before the `BufferingAudioReader`/`ThreadedWriter` that uses it**, or
  before the `ThreadedWriter` has flushed — data loss or a use-after-free.
- **Registering formats on the audio thread**, or registering the same format name twice (debug assert).
- **Non-seekable stream into `AudioFormatManager::createReaderFor`** — every format attempt needs the
  position restored; a `stdin`/socket-backed stream asserts.
- **`writeFromAudioSource` with an unprepared `AudioSource`** — you must call
  `prepareToPlay (samplesPerBlock, sr)` on it first; nothing does it for you.
- **Expecting `writeFromAudioReader` to resample** — it converts float/fixed point only, never sample rate.
- **The ARA readers** (`format/juce_ARAAudioReaders.h`) are gated behind `JucePlugin_Enable_ARA` and pull
  in `juce_audio_processors`; they are not part of the plain file I/O path.
