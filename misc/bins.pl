#!/usr/bin/env perl
use 5.036;

use Getopt::Long ();
use List::Util ();
use POSIX ();

sub key2freq ( $n, $s ) {
    state $r = { 61 => 33, 88 => 48 };
    my $o = $n - ( $r->{$s} || die "don't know manuals with $s keys\n" );
    return 440 * ( 2 ** ( $o / 12 ) );
}

Getopt::Long::GetOptions(
    'sample_rate=i'     => \my $sample_rate,
    'keyboard_size=i'   => \my $keyboard_size,
    'tolerance=f'       => \my $tolerance,
);
$sample_rate ||= 44100;
$keyboard_size ||= 61;
$tolerance ||= 1;

my $highest_error = 0;
for my $key ( 0 .. $keyboard_size - 1 ) {
    my $old_freq = key2freq( $key, $keyboard_size );
    my $bandwidth = 2 * ( key2freq( $key + 0.5 * $tolerance, $keyboard_size ) - $old_freq );
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

    my $error = 100 * ( $new_freq - $old_freq ) / $bandwidth;
    $highest_error = List::Util::max( $highest_error, abs( $error ) );
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
