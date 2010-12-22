<?php
require_once '/WWW/email.php';

//TITLE=OPL3 MIDI player for Linux and Cygwin

$title = 'OPL3 MIDI player for Linux and Cygwin';
$progname = 'adlmidi';
$git = 'git://bisqwit.iki.fi/adlmidi.git';

$text = array(
   'purpose:1. Purpose' => "

AdlMIDI is a commandline program that plays MIDI files
using software OPL3 emulation.

", 'features:1. Key features' => "

<ul>
 <li>OPL3 emulation with four-operator mode support</li>
 <li>FM patches from a number of known PC games, copied from
   files typical to
   AIL / Miles Sound System / DMX / Human Machine Interfaces
   / Creative IBK.</li>
 <li>Stereo sound</li>
 <li>Reverb filter based on code from <a href=\"http://sox.sourceforge.net/\">SoX</a>,
     based on code from <a href=\"http://freeverb3.sourceforge.net/\">Freeverb</a>.
     A copy of either project is not needed.</li>
 <li>Number of simulated soundcards can be specified as 1-100 (maximum channels 1800!)</li>
 <li>xterm-256color support</li>
 <li>Pan (binary panning, i.e. left/right side on/off)</li>
 <li>Pitch-bender with adjustable range</li>
 <li>Vibrato that responds to RPN/NRPN parameters</li>
 <li>Sustain enable/disable</li>
 <li>MIDI and RMI file support</li>
 <li>loopStart / loopEnd tag support (Final Fantasy VII)</li>
</ul>

<hr>
This project is <b>developed in C++</b>, but a <i>GW-BASIC version</i> is also provided
(<a href=\"http://bisqwit.iki.fi/jutut/kuvat/programming_examples/midiplay.html\"
>MIDIPLAY.BAS</a>).
This player was
<a href=\"http://www.youtube.com/watch?v=ZwcFV3KrnQA\"
>first implemented in GW-BASIC for a Youtube demonstration video</a>!
With alterations that take 15 seconds to implement,
it can be run in QBasic and QuickBASIC too.

", 'copying:1. Copying and contributing' => "

cromfs has been written by Joel Yliluoma, a.k.a.
<a href=\"http://iki.fi/bisqwit/\">Bisqwit</a>,<br />
and is distributed under the terms of the
<a href=\"http://www.gnu.org/licenses/gpl-3.0.html\">General Public License</a>
version 3 (GPL3).
 <br/>
The OPL3 emulator within is from DOSBox is licensed under GPL2+.
 <br/>
The FM soundfonts (patches) used in the program are
imported from various PC games without permission
from their respective authors.
 <p/>
Patches and other related material can be submitted to the
author
".GetEmail('by e-mail at:', 'Joel Yliluoma', 'bisqwi'. 't@iki.fi')."
 <p/>
The author also wishes to hear if you use adlmidi, and for what you
use it and what you think of it.

");
include '/WWW/progdesc.php';
