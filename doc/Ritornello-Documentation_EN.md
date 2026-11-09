# Ritornello – Documentation

*As of: 2026-11-09 · Loop expander for Sofonisba and Lucia · 12 HP · Plugin "reminiscence of the unattained"*

> **Note:** The plugin is an independent work, strongly inspired by the wonderful world of Buchla modules. It has no connection to Buchla, Tiptop Audio, or any other manufacturer. All trademarks belong to their respective owners. The implementation is original; the source of the idea is a publicly available reference documentation.

<a id="ritornello-overview"></a>

## 1. Overview

Ritornello extends Sofonisba and Lucia with **loops with a counter**. Until now, a First/Last segment ran endlessly in a circle. With Ritornello you mark a **Begin stage** and an **End stage** via gate inputs. When the section leaves the End stage, it jumps back to the Begin stage and counts down as it does so. After the last pass it continues normally. Loops may be nested.

The module has no sound generation and no logic of its own. Like Amilcare and Bianca, it is a carrier for jacks and displays; the loop logic runs in Sofonisba or Lucia.

**Name**<br>
Ritornello is the musical term for the recurring passage. The naming scheme of the Anguissola family is exhausted and is deliberately abandoned here.

**What you get**<br>
Three inputs (Begin, End, Count), a digit display per section, a remainder output (REST), an exit pulse (EXIT), and an "overall loop done" output (DONE).

**When nothing is patched**<br>
Without a docked Ritornello, or as long as Begin and End are not both patched, Sofonisba behaves bit-identically to how it does without the loop function.

<a id="ritornello-mounting"></a>

## 2. Mounting and Docking

Ritornello docks to the left or right of Sofonisba or Lucia. The LINK light below the module name shows whether a host was detected.

**Chain**<br>
Starting from the direct neighbor, the host keeps searching through other expanders (Amilcare, Bianca, Ritornello), up to three modules per side in total. A third-party module or a gap ends the chain.

**Several Ritornellos on one host**<br>
Only the nearest one is used, left before right. The more distant ones have no effect, and their LINK light stays off.

**Without a connection**<br>
If you detach Ritornello from the host, it resets outputs, displays, and the LINK light.

<a id="ritornello-panel"></a>

## 3. Panel

At the top are the title and the LINK light. Below that is one column per section (A on the left, B on the right), followed by the shared inputs.

**Digit display A/B**<br>
Shows the remainder of the innermost loop that contains the section's current stage, see [Section 7](#ritornello-display) ([[#7. Remainder, Display, and Exit|Obsidian]]).

**REST A/B**<br>
CV output, 1 V per remaining pass, same as the display.

**EXIT A/B**<br>
Trigger output (1 ms), fires on every regular exit from a loop.

**DONE A/B**<br>
Trigger output (1 ms), fires only on a regular exit from the **outermost** loop ("overall loop done"). The jacks sit in their own row below EXIT.

**LOOP BEGIN**<br>
Poly gate input, channel 1–16 = stage 1–16. A high (from 2 V) marks the first stage of a loop.

**LOOP END**<br>
Poly gate input, channel 1–16 = stage 1–16. A high (from 2 V) marks the last stage of a loop.

**LOOP COUNT**<br>
Poly CV input, channel 1–16 = stage 1–16, effective on the channel of the End stage. 1 V per repetition, 0 V = endless.

<a id="ritornello-loops"></a>

## 4. Loops: Begin and End

**Both jacks required**<br>
Loops only come into being when **LOOP BEGIN and LOOP END are both patched**. If either jack is empty, the sequence stays linear.

**Gate level**<br>
Threshold 2 V, no hysteresis (like the stage modifier gates of Amilcare). Only the channels the cable actually carries are read. A mono cable therefore marks only stage 1.

**Shared by A and B**<br>
The markers apply to both sections, just like the stage programming itself. The counter states run separately per section.

**Independent of First and Last**<br>
Begin and End are markers of their own. Start/End-of-Cycle triggers and BEATS_FACTOR segments remain untouched. If the End stage is also on Last, the normal First/Last wrap applies after exiting.

**Pairing by bracketing**<br>
Each End closes the most recently opened Begin that is still open. It follows that:

- Begin 2, Begin 3, End 4, End 5: **nested**. The inner loop spans stages 3–4, the outer one stages 2–5.
- Begin 2, End 3, Begin 4, End 5: **two separate** loops (2–3 and 4–5).
- Begin and End on the same stage: a **single-stage loop**. The stage repeats.
- Unpaired Begin or End markers are ignored.
- Each stage has at most one Begin and one End. Two loops with the same Begin or End stage are therefore not possible.

<a id="ritornello-counter"></a>

## 5. The Counter

The counter sits on the loop's **End stage**: the Count value on the End stage's channel applies to the loop that ends there.

**Voltage = repetitions**<br>
1 V corresponds to one repetition. One repetition means one additional pass.

| Voltage on the End stage's channel | Repetitions | Passes |
|---|---|---|
| below 1 V (also negative, not patched) | – | endless |
| 1 V to below 2 V | 1 | 2 |
| 2 V to below 3 V | 2 | 3 |
| … | … | … |
| 9 V and above | 9 | 10 |

Rule of thumb: passes = floor(V) + 1, repetitions = floor(V). A loop with exactly one pass does not exist; the smallest counted loop runs twice.

**Unpatched Count**<br>
If LOOP COUNT is not patched or the cable does not carry this channel, the loop is endless. Begin and End alone therefore yield an endless loop.

**Mono cable**<br>
A mono cable on LOOP COUNT only affects stage 1. For several loops you need a poly cable with at least as many channels as the last End stage is high.

**Read time**<br>
Counted loops read the value when the counter is loaded. A change during playback only takes effect on the next load. **Endless loops keep re-reading the value.** If a value of 1 V or more is applied later, it takes effect immediately, and the pass currently running counts as the first. This lets you turn an endless loop into a counted one during live play.

**End stage moved**<br>
If you move an End to another stage, the Count value has to move with it. Otherwise the new End channel carries 0 V, and the loop runs endlessly.

<a id="ritornello-example"></a>

## 6. A Continuous Example

Setup: **Begin** on stages 2 and 3, **End** on stages 4 and 5. Count on channel 4 = **1.5 V** (inner loop, 2 passes) and on channel 5 = **3.5 V** (outer loop, 4 passes). The inner loop spans stages 3–4, the outer one stages 2–5.

**Stage sequence**<br>
1, then four times the sequence 2 3 4 3 4 5, then 6.

The table shows what the display and the REST output deliver along the way. The character "–" stands for 0 V (no loop).

| Outer pass | Stage 2 | Stage 3 | Stage 4 | Stage 3 | Stage 4 | Stage 5 |
|---|---|---|---|---|---|---|
| 1 | 4 | 2 | 2 | 1 | 1 | 4 |
| 2 | 3 | 2 | 2 | 1 | 1 | 3 |
| 3 | 2 | 2 | 2 | 1 | 1 | 2 |
| 4 | 1 | 2 | 2 | 1 | 1 | 1 |

Stage 1 and stage 6 are in no loop and show "–".

**EXIT pulses**<br>
When leaving stage 4 in the second pass of the inner loop, EXIT fires, in **each** of the four outer passes. When leaving stage 5 in the fourth pass, EXIT fires once more for the outer loop. That makes five pulses in total.

**Inner loop reloads every time**<br>
As soon as the section enters stage 5 or stage 2, it lies outside the inner loop; its counter expires and is fully reloaded on the next entry into stage 3. This is why the inner loop runs twice in every outer pass.

<a id="ritornello-display"></a>

## 7. Remainder, Display, and Exit

**Remainder**<br>
REST and the digit display show the **remaining passes of the innermost loop** that contains the section's current stage, **including the running one**. On the first pass it shows the total, on the last pass a 1. The REST output delivers 1 V per pass, at most 10 V. 0 V means: no loop, endless, or Pendulum. The CV output deliberately does not blink.

**Digit display**<br>
Shows 1 to 10. For endless or no loop, a "–" is shown (drawn as a bar).

**Blinking 0 on exit**<br>
When the section leaves a loop regularly, the display blinks a 0: for 0.5 s, 0.1 s on and 0.1 s off (on, off, on, off, on). After that, the value of the new state is shown. After leaving an inner loop, that is the value of the outer one; after leaving the outermost loop, a "–".

**EXIT**<br>
A 1 ms trigger (10 V) on **every** regular exit, including inner loops. This way there is always feedback. A stage's MOD_MUTE does not suppress EXIT (structural marker, like End of Stage).

**DONE (overall loop done)**<br>
A 1 ms trigger (10 V) that fires only on a regular exit from the **outermost** loop, that is, when no other loop encloses the stage upon leaving. Inner loops do not trigger DONE. Endless loops never exit regularly, so DONE stays silent. Like EXIT, DONE is independent of MOD_MUTE. In the example above, DONE fires exactly once: when leaving stage 5 in the fourth outer pass, whereas EXIT fires five times.

**Scope note**<br>
A 1 ms pulse is tight on a scope. For tests, an LED, an envelope, or a gate extender is more reliable.

<a id="ritornello-special"></a>

## 8. State Rule and Special Cases

**State rule**<br>
A loop's counter is only active while the current stage lies between Begin and End. On every stage entry, including Strobe, Recall, Reset, and Continuous, the counters of all loops whose range does not contain the new stage expire. They reload fully when next needed.

**Strobe, Recall, Reset**<br>
Strobing into a loop starts with a full counter. Recalling out of a loop lets its counter expire. A jump within the loop lets the counter keep running. Reset reloads everything, even within the same loop. An endless loop, by contrast, picks up a newly applied Count through its continuous re-reading, see [Section 5](#ritornello-counter) ([[#5. The Counter|Obsidian]]).

**Continuous**<br>
With Continuous addressing, the address determines the stage. The loop jumps do not apply there, but the state rule above still holds.

**Skip**<br>
A skipped End stage counts when left, and the Begin stage repeats accordingly often. Example: stage 3 is End and on Skip, Begin is on stage 2, Count 3.5 V (4 passes): the sequence is 1, 2, 2, 2, 2, 4, 5, 6. If the Begin stage is on Skip, the jump lands there and moves on immediately. An endless loop consisting only of Skip stages does not hang; skipping is limited to 16 steps.

**Mute**<br>
Mute only suppresses the stage's P1/P2/APOUT pulses. Loops, EXIT, and DONE remain unaffected.

**Stop, Sustain, Enable, Advance**<br>
Their precedence rules remain unchanged. A loop jumps when the End stage is left, whether by elapsed time or by an Advance trigger.

**Reverse**<br>
Begin and End swap roles. The Begin stage is what gets left, the jump goes to the End stage, and the counter remains that of the End stage. This behavior is confirmed in Rack and stays as is.

**Pendulum**<br>
Loops are inactive in Pendulum. REST delivers 0 V, the display "–".

**Global Clear**<br>
Global Clear resets the loop counters. The markers come from the cables and remain unaffected.

**Presets**<br>
Begin, End, and Count are present only at the cables. They are stored neither in the presets nor as stage data; the patch keeps the cabling.

<a id="ritornello-pitfalls"></a>

## 9. Pitfalls

**Loop runs endlessly**<br>
The most common cause: Count not patched, a mono cable on LOOP COUNT (only affects stage 1), Count on the wrong channel (it belongs on the **End** stage), or less than 1 V on the channel. See [Section 5](#ritornello-counter) ([[#5. The Counter|Obsidian]]).

**Nothing jumps at all**<br>
Begin and End must both be patched. A missing jack leaves the sequence linear.

**Loop jumps to the wrong stage**<br>
Pairing happens by bracketing. With nested loops, each End closes the most recently opened Begin, see [Section 4](#ritornello-loops) ([[#4. Loops: Begin and End|Obsidian]]).

**The number stays put despite a Count change**<br>
Counted loops read the Count on loading. Leave the loop or trigger a Reset so the new value is read.

**EXIT not visible on the scope**<br>
The pulse lasts only 1 ms.

**"One repetition" is not "one pass"**<br>
1 V means two passes. If you compare the display with the Count: the display counts passes, the Count counts repetitions.

<a id="ritornello-technical"></a>

## 10. Technical Data

| Property | Value |
|---|---|
| Width | 12 HP |
| Gate threshold (Begin, End) | 2 V, no hysteresis |
| Count mapping | floor(V), limited to 0…9 repetitions |
| Counters per section | separate, markers shared |
| REST output | 1 V per pass, at most 10 V |
| EXIT pulse | 1 ms, 10 V |
| DONE pulse (overall loop done) | 1 ms, 10 V, outermost loop only |
| Blinking | 0.5 s total, 0.1 s phases |
| Chain | up to 3 expanders per side |

**Behavior without the loop function**<br>
Without a docked Ritornello or without both cables, the engine receives zero data and behaves bit-identically to the state without the loop function (verified against the previous engine).

<a id="ritornello-decisions"></a>

## 11. Decisions and Open Points

Decisions by me, in the order of implementation:

1. **A module of its own from the start**, no space in Amilcare. Pure jack carrier without DSP of its own; the logic resides in the Sofonisba engine.
2. **Begin and End are markers of their own**, independent of First and Last.
3. **Nesting** is part of version 1. Each End stage has its own counter.
4. **Value range** (changed 2026-11-02): 1 V per repetition instead of 1 V per pass. 0 V = endless.
5. **Configuration on the End stage**, poly channel = stage number, shared by A and B.
6. **Reverse:** Begin and End swap roles. Confirmed in Rack and left as is.
7. **Strobe in, Recall out** (as understood): the state rule follows from this.
8. **Remainder and display show the innermost loop.**
9. **Exit pulse and remainder display** both stay in version 1. **EXIT also fires for inner loops**, so there is always feedback. On exit, the display blinks a real, alternately lit and unlit 0.
10. **Endless loops keep re-reading the Count**, so a cable plugged in during live play takes effect immediately.
11. **Pendulum:** loops inactive in version 1.
12. **Name Ritornello:** the naming scheme of the Anguissola family is deliberately abandoned.
13. **Chain search** (up to three expanders per side): Amilcare, Bianca, and Ritornello work simultaneously on one Sofonisba. Checked in Rack, without errors.
14. **Two-digit display "10"** (at 9 V and above): fits on the display, checked in Rack.
15. **DONE output "overall loop done"**: fires only on a regular exit from the outermost loop, endless loops stay silent, 1 ms trigger like EXIT, independent of MOD_MUTE, two new jacks (A/B) in their own row below EXIT (grid 44/56/68 mm; there is no room next to EXIT at 12 HP). New outputs appended at the end of the enum so existing patches do not shift.

**Open**
- Whether the counter configuration should later also be possible via a knob on the expander.
- Jumps, a second voltage (CV 2), time quantization, clock division, and Fix remain separate, later features.
