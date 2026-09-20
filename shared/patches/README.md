# Patches for the third-party checkouts in `CLAP/`

`CLAP/` is gitignored — every checkout there is an independent upstream clone
that each developer makes themselves. Anything that has to be changed in one of
them therefore lives here as a patch, or it is lost on the next fresh clone.

Apply one with:

```sh
cd CLAP/<checkout>
git apply ../../shared/patches/<patch>
```

## `clap-wrapper-vst3-sdk-3.8.patch`

Against `free-audio/clap-wrapper` v0.16.0, needed to build against VST 3.8 or
newer — which is the first MIT-licensed VST3 SDK, and therefore the only one the
suite can use.

VST 3.8 added `FObject::iid`, and at the same time renamed the parameter of the
`DEFINE_INTERFACES` macro from `iid` to `_iid`. The wrapper's `wrapasvst3.h`
writes its dynamic `queryInterface` block by hand and still says `iid`, which
now resolves ambiguously between the macro's parameter and the new
`FObject::iid` member, so the wrapper does not compile. The patch spells those
six references `_iid`.

It is upstream's bug rather than ours, and has not been reported yet. The patch
makes the wrapper require VST3 3.8+; it will not build against 3.7 with it
applied, which is fine because 3.7 is not MIT.

## `clap-wrapper-linux-timer-uaf.patch`

Against `free-audio/clap-wrapper` v0.16.0. Fixes a use-after-free in the Linux
run-loop timer bookkeeping that crashes the host when a plugin's window is
closed and opened again -- which is what switching between two plugins in a
REAPER FX chain does.

`ClapAsVst3::register_timer` reuses a `TimerObject` slot whose period is 0, and
dropped that slot's `TimerHandler` with `to.handler.reset()` without first
calling `IRunLoop::unregisterTimer` on it. The host does not own the handler, so
it kept a raw pointer to the deleted object and called `onTimer()` on it at the
next tick. The sibling function `unregister_timer` already unregisters before
resetting; this makes `register_timer` do the same.

The second hunk stops `attachTimers` from registering a slot whose period is 0.
Such a slot is one the plugin has unregistered, and asking the host to tick it
means a timer callback for an id the plugin no longer knows, as fast as the host
will run it.

Found with a backtrace from REAPER 7.80 on Linux: the crash is in REAPER's timer
list, calling vtable slot 3 (`ITimerHandler::onTimer`) on a freed object.

Not reported upstream yet.
