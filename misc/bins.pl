#!/usr/bin/env perl
use 5.020;
use experimental qw( signatures );
use warnings qw( all );
no warnings qw( experimental::signatures );

use POSIX ();

sub key2freq ( $n ) {
    return 440 * ( 2 ** ( ( $n - 33 ) / 12 ) );
}

my $sample_rate = $ARGV[0] || 44100;

for my $key ( 0 .. 60 ) {
    my $old_freq = key2freq( $key );
    my $bandwidth = 2 * ( key2freq( $key + 0.5 ) - $old_freq );
    my $N = POSIX::floor( $sample_rate / $bandwidth );
    my $k = POSIX::floor( $old_freq / $bandwidth );

    my $delta = abs( $sample_rate * ( $k / $N ) - $old_freq );
    for ( my $i = $N - 1;; $i--) {
        my $tmp_delta = abs( $sample_rate * ( $k / $i ) - $old_freq );
        if ( $tmp_delta < $delta ) {
            $delta = $tmp_delta;
            $N = $i;
        } else {
            last;
        }
    }
    my $new_freq = $sample_rate * ( $k / $N );
    my $new_bandwidth = $sample_rate / $N;

    printf "%d\t%f\t%f\t%f\t%f\t%f\t%d/%d\n",
        $key,
        $old_freq,
        $new_freq,
        $bandwidth,
        $new_bandwidth,
        100 * ( $new_freq - $old_freq ) / $bandwidth,
        $k,
        $N,
    ;
}
