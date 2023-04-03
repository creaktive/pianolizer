#!/usr/bin/env perl
use 5.036;

use Getopt::Long qw(GetOptions);
use List::Util qw(max);
use POSIX qw(floor);

sub key2freq($n, $s, $p, $o) {
    state $r = { 61 => 33, 88 => 48 };
    $o ||= $r->{$s} || die "don't know manuals with $s keys; specify --offset\n";
    return $p * (2 ** (($n - $o) / 12));
}

my $sample_rate = 44100;
my $keyboard_size = 61;
my $tolerance = 1.0;
my $offset;
my $pitch = 440.0;
GetOptions(
    'sample_rate=i'     => \$sample_rate,
    'keyboard_size=i'   => \$keyboard_size,
    'tolerance=f'       => \$tolerance,
    'offset=i'          => \$offset,
    'pitch=f'           => \$pitch,
);

my $highest_error = 0;
for my $key (0 .. $keyboard_size - 1) {
    my $old_freq = key2freq($key, $keyboard_size, $pitch, $offset);
    my $bandwidth = 2 * (key2freq($key + 0.5 * $tolerance, $keyboard_size, $pitch, $offset) - $old_freq);
    my $N = floor($sample_rate / $bandwidth);
    my $k = floor($old_freq / $bandwidth);

    my $delta = abs($sample_rate * ($k / $N) - $old_freq);
    for (my $i = $N - 1;; $i--) {
        my $tmp_delta = abs($sample_rate * ($k / $i) - $old_freq);
        if ($tmp_delta < $delta) {
            $delta = $tmp_delta;
            $N = $i;
        } else {
            last;
        }
    }
    my $new_freq = $sample_rate * ($k / $N);
    my $new_bandwidth = $sample_rate / $N;

    my $error = 100 * ($new_freq - $old_freq) / $bandwidth;
    $highest_error = max $highest_error, abs($error);
    printf "%d\t%f\t%f\t%f\t%f\t%f\t%d/%d\n",
        $key,
        $old_freq,
        $new_freq,
        $bandwidth,
        $new_bandwidth,
        $error,
        $k,
        $N,
    ;
}

say STDERR join "\t", $sample_rate, $keyboard_size, $highest_error;
exit;
