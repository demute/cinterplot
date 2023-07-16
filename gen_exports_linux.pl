#!/usr/bin/env perl
use strict;

print <<EOF;
{
    /* File automatically generated from exports_mac.txt, do not edit */
    global:
        /* Exported functions */
EOF

for (<>)
{
    chop;
    print "        " . $_ . ";\n"
}

print <<EOF;
        /* more function names here... */

        /* Exported variables */
        _main;

    /* Hide all other symbols */
    local:
        *;
};
EOF
