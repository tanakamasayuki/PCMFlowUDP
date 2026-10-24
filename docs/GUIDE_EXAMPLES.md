# PCMFlowUDP — Example workflow

> 日本語版: [GUIDE_EXAMPLES.ja.md](GUIDE_EXAMPLES.ja.md)

Carry PCM and encoded audio over UDP.

Check installation and platform requirements in the [library README](../README.md), then choose a sketch from the [example index](../examples/README.md). Version/support tables and internal capacity defaults stay in their original reference rather than being duplicated here.

## From setup to a verified result

1. Choose the smallest relevant sketch and check its board, hardware, dependencies and bundled files.
2. Match pins, peers and input files to your actual setup. Each README identifies important placeholders.
3. Open the .ino, select board/port and upload. Match Serial baud where used.
4. Locate the last completed stage: initialization, input, processing or output. Compilation does not test hardware I/O.
5. Reproduce the supplied example before changing one setting at a time.

## API and data handling

Set local Wi-Fi credentials and the receiving peer address/port. Start the UDP socket before protocol send/receive and poll incoming packets regularly. Raw UDP, VBAN and RTP have different framing and format rules; both sides must agree.

UDP does not guarantee delivery, ordering or retry, so a network send result is not a playback acknowledgement. The microphone/speaker portions in these examples include placeholders: silence transmission or received byte counts alone do not prove live audio. Replace those sections with hardware I/O while retaining sample/byte units and pacing.

RTP timestamps follow the payload clock; do not assume every codec uses the PCM sampling clock (G.722 is a notable case). Manage packet size, partial output and buffering explicitly.

## When adapting it

- Distinguish bool status, processed counts and handles; do not use stale output after failure.
- Registered callbacks run later from their documented context. Decide synchronization and retained-data lifetime.
- Keep bytes, samples, frames and pixels distinct; capacity is different from valid length.
- Retain views/pointers/buffers for the required period. Copying a pointer does not copy its payload.
- Add application-specific failure handling, retry and cleanup that a minimal example may omit.

## Read the individual README

Use Prepare and run for hardware and steps, Follow the sketch for the actual APIs and precautions for feature-specific constraints. If output is missing, check initialization first, continued service calls next and input/peer readiness after that.
