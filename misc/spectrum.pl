#!/usr/bin/env perl
use 5.036;

use JSON::PP ();
use POSIX ();

# use constant PI => 3.1415926535897932384626433832795;
#
# sub rainbow($p) {
#     return [
#         POSIX::floor(128 + 127 * sin(PI + $p)),
#         POSIX::floor(128 + 127 * sin(PI + $p + 2 * PI / 3)),
#         POSIX::floor(128 + 127 * sin(PI + $p + 4 * PI / 3)),
#     ];
# }
#
# my @colors;
# for (my $i = 0; $i < 2 * PI; $i += (PI / 6)) {
#    unshift @colors => rainbow($i);
# }
# push @colors => shift @colors;
# push @colors => shift @colors;

my @colors = (
    [0xff, 0x00, 0x00], # red
    [0xff, 0x80, 0x00], # orange
    [0xff, 0xff, 0x00], # yellow
    [0x80, 0xff, 0x00], # chartreuse
    [0x00, 0xff, 0x00], # green
    [0x00, 0xff, 0x80], # spring
    [0x00, 0xff, 0xff], # cyan
    [0x00, 0x80, 0xff], # azure
    [0x00, 0x00, 0xff], # blue
    [0x80, 0x00, 0xff], # violet
    [0xff, 0x00, 0xff], # magenta
    [0xff, 0x00, 0x80], # rose
);

say JSON::PP::encode_json(\@colors);
exit;

my $scale = 64;
my @rainbow;
for my $c (@colors) {
    for (1 .. $scale) {
        push @rainbow =>
            sprintf '%3d %3d %3d', @$c
    }
}

print << "END_PPM";
P3
@{[ scalar @rainbow ]} $scale
255
END_PPM

say join("\t", @rainbow)
    for 1 .. $scale;
