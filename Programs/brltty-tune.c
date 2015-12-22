/*
 * BRLTTY - A background process providing access to the console screen (when in
 *          text mode) for a blind person using a refreshable braille display.
 *
 * Copyright (C) 1995-2015 by The BRLTTY Developers.
 *
 * BRLTTY comes with ABSOLUTELY NO WARRANTY.
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any
 * later version. Please see the file LICENSE-GPL for details.
 *
 * Web Page: http://brltty.com/
 *
 * This software is maintained by Dave Mielke <dave@mielke.cc>.
 */

#include "prologue.h"

#include <stdio.h>
#include <string.h>

#include "log.h"
#include "options.h"
#include "prefs.h"
#include "tune_utils.h"
#include "tune_build.h"
#include "notes.h"
#include "datafile.h"

static int opt_fromFiles;
static char *opt_outputVolume;
static char *opt_tuneDevice;

#ifdef HAVE_MIDI_SUPPORT
static char *opt_midiInstrument;
#endif /* HAVE_MIDI_SUPPORT */

BEGIN_OPTION_TABLE(programOptions)
  { .letter = 'f',
    .word = "files",
    .setting.flag = &opt_fromFiles,
    .description = "Use files rather than command line arguments."
  },

  { .letter = 'v',
    .word = "volume",
    .argument = "loudness",
    .setting.string = &opt_outputVolume,
    .description = "Output volume (percentage)."
  },

  { .letter = 'd',
    .word = "device",
    .argument = "device",
    .setting.string = &opt_tuneDevice,
    .description = "Name of tune device."
  },

#ifdef HAVE_PCM_SUPPORT
  { .letter = 'p',
    .word = "pcm-device",
    .flags = OPT_Hidden,
    .argument = "device",
    .setting.string = &opt_pcmDevice,
    .description = "Device specifier for soundcard digital audio."
  },
#endif /* HAVE_PCM_SUPPORT */

#ifdef HAVE_MIDI_SUPPORT
  { .letter = 'm',
    .word = "midi-device",
    .flags = OPT_Hidden,
    .argument = "device",
    .setting.string = &opt_midiDevice,
    .description = "Device specifier for the Musical Instrument Digital Interface."
  },

  { .letter = 'i',
    .word = "instrument",
    .argument = "instrument",
    .setting.string = &opt_midiInstrument,
    .description = "Name of MIDI instrument."
  },
#endif /* HAVE_MIDI_SUPPORT */
END_OPTION_TABLE

static void
beginTuneStream (const char *name, void *data) {
  TuneBuilder *tb = data;
  resetTuneBuilder(tb);
  setTuneSourceName(tb, name);
}

static void
playTune (TuneBuilder *tb) {
  ToneElement *tune = getTune(tb);

  if (tune) {
    tunePlayTones(tune);
    tuneSynchronize();
    free(tune);
  }
}

static void
endTuneStream (int incomplete, void *data) {
  if (!incomplete) {
    TuneBuilder *tb = data;
    playTune(tb);
  }
}

static
DATA_OPERANDS_PROCESSOR(processTuneOperands) {
  DataOperand line;

  if (getDataText(file, &line, NULL)) {
    DataString text;

    if (parseDataString(file, &text, line.characters, line.length, 0)) {
      return parseTuneText(data, text.characters);
    }
  }

  return 1;
}

static
DATA_OPERANDS_PROCESSOR(processTuneLine) {
  TuneBuilder *tb = data;
  incrementTuneSourceIndex(tb);

  BEGIN_DATA_DIRECTIVE_TABLE
    DATA_NESTING_DIRECTIVES,
    DATA_VARIABLE_DIRECTIVES,
    DATA_CONDITION_DIRECTIVES,
    {.name=NULL, .processor=processTuneOperands},
  END_DATA_DIRECTIVE_TABLE

  return processDirectiveOperand(file, &directives, "tune file directive", tb);
}

int
main (int argc, char *argv[]) {
  {
    static const OptionsDescriptor descriptor = {
      OPTION_TABLE(programOptions),
      .applicationName = "brltty-tune",
      .argumentsSummary = "note... | -f [{file | -}...]"
    };
    PROCESS_OPTIONS(descriptor, argc, argv);
  }

  resetPreferences();
  if (!parseTuneDevice(opt_tuneDevice)) return PROG_EXIT_SYNTAX;
  if (!parseTuneVolume(opt_outputVolume)) return PROG_EXIT_SYNTAX;

#ifdef HAVE_MIDI_SUPPORT
  if (!parseTuneInstrument(opt_midiInstrument)) return PROG_EXIT_SYNTAX;
#endif /* HAVE_MIDI_SUPPORT */

  if (!setTuneDevice()) return PROG_EXIT_SEMANTIC;
  ProgramExitStatus exitStatus = PROG_EXIT_FATAL;
  TuneBuilder *tb = newTuneBuilder();

  if (tb) {
    if (opt_fromFiles) {
      const InputFilesProcessingParameters parameters = {
        .beginStream = beginTuneStream,
        .endStream = endTuneStream,
        .processLine = processTuneLine,
        .data = tb
      };

      exitStatus = processInputFiles(argv, argc, &parameters);
    } else if (argc) {
      exitStatus = PROG_EXIT_SUCCESS;
      setTuneSourceName(tb, "<command-line>");

      do {
        incrementTuneSourceIndex(tb);
        if (!parseTuneString(tb, *argv)) break;
        argv += 1;
      } while (argc -= 1);

      playTune(tb);
    } else {
      logMessage(LOG_ERR, "missing tune");
      exitStatus = PROG_EXIT_SYNTAX;
    }

    if (exitStatus == PROG_EXIT_SUCCESS) {
      switch (getTuneStatus(tb)) {
        case TUNE_STATUS_OK:
          exitStatus = PROG_EXIT_SUCCESS;
          break;

        case TUNE_STATUS_SYNTAX:
          exitStatus = PROG_EXIT_SYNTAX;
          break;

        case TUNE_STATUS_FATAL:
          exitStatus = PROG_EXIT_FATAL;
          break;
      }
    } else if (exitStatus == PROG_EXIT_FORCE) {
      exitStatus = PROG_EXIT_SUCCESS;
    }

    destroyTuneBuilder(tb);
  }

  return exitStatus;
}

#include "alert.h"

void
alert (AlertIdentifier identifier) {
}
