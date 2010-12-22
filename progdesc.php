<?php
require_once '/WWW/email.php';

//TITLE=OPL3 MIDI player for Linux and Cygwin

$title = 'OPL3 MIDI player for Linux and Cygwin';
$progname = 'adlmidi';
$git = 'git://bisqwit.iki.fi/adlmidi.git';

$text = array(
   '1. Purpose' => "

This program plays MIDI files using OPL3 emulation.

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
