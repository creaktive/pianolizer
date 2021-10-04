#!/usr/bin/env perl
use 5.020;
use experimental qw( signatures );
use warnings qw( all );
no warnings qw( experimental::signatures );

use POSIX ();

sub key2freq ( $n ) {
    return 440 * ( 2 ** ( ( $n - 48 ) / 12 ) );
}

my $sample_rate = 44100;

for my $key ( 0 .. 87 ) {
    my $freq = key2freq( $key );
    my $bandwidth = 2 * ( $freq - key2freq( $key - 0.5 ) );
    my $N = POSIX::ceil( $sample_rate / $bandwidth );
    my $k = POSIX::ceil( $freq / $bandwidth );

    my $new_freq;
    do {
        $N++;
        $new_freq = $sample_rate * ( $k / $N );
    } until ( $new_freq - $freq <= 0 );
    my $new_bandwidth = $sample_rate / $N;

    printf "%d\t%f\t%f\t%d\t%f\t%f\t%f\n",
        $key,
        $freq,
        $bandwidth,
        $N,
        $new_freq,
        $new_bandwidth,
        100 * ( $freq - $new_freq ) / $bandwidth,
    ;
}
