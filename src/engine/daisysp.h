// Local shim (not part of upstream DaisySP): pulls in only the DaisySP modules
// this plugin uses, so the vendored subset in third_party/ is enough.
// MIT-licensed DaisySP modules: third_party/daisysp   (see its LICENSE)
// LGPL-2.1 DaisySP-LGPL modules: third_party/daisysp-lgpl (see its LICENSE)
#pragma once
#include <cstddef>
#include "../../third_party/daisysp/dsp.h"
#include "../../third_party/daisysp/delayline.h"
#include "../../third_party/daisysp/crossfade.h"
#include "../../third_party/daisysp/limiter.h"
#include "../../third_party/daisysp/overdrive.h"
#include "../../third_party/daisysp/oscillator.h"
#include "../../third_party/daisysp-lgpl/reverbsc.h"
#include "../../third_party/daisysp-lgpl/tone.h"
