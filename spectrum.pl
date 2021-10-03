#!/usr/bin/env perl
use 5.020;
use experimental qw( signatures );
use warnings qw( all );
no warnings qw( experimental::signatures );

use Carp ();
use POSIX ();

# Ported to Perl from: https://github.com/ArashPartow/bitmap/blob/master/bitmap_image.hpp
# Credits: Dan Bruton http://www.physics.sfasu.edu/astro/color.html
sub convert_wave_length_nm_to_rgb ($wave_length_nm, $gamma = 0.8) {
    my ( $red, $green, $blue );

    if ( ( 380 <= $wave_length_nm ) && ( $wave_length_nm < 440 ) ) {
        $red    = -( $wave_length_nm - 440 ) / ( 440 - 380 );
        $green  = 0;
        $blue   = 1;
    } elsif ( ( 440 <= $wave_length_nm ) && ( $wave_length_nm < 490 ) ) {
        $red    = 0;
        $green  = ( $wave_length_nm - 440 ) / ( 490 - 440 );
        $blue   = 1;
    } elsif ( ( 490 <= $wave_length_nm ) && ( $wave_length_nm < 510 ) ) {
        $red    = 0;
        $green  = 1;
        $blue   = -( $wave_length_nm - 510 ) / ( 510 - 490 );
    } elsif ( ( 510 <= $wave_length_nm ) && ( $wave_length_nm < 580 ) ) {
        $red    = ($wave_length_nm - 510) / (580 - 510);
        $green  = 1;
        $blue   = 0;
    } elsif ( ( 580 <= $wave_length_nm ) && ( $wave_length_nm < 645 ) ) {
        $red    = 1;
        $green  = -( $wave_length_nm - 645 ) / ( 645 - 580 );
        $blue   = 0;
    } elsif ( ( 645 <= $wave_length_nm ) && ( $wave_length_nm <= 780 ) ) {
        $red    = 1;
        $green  = 0;
        $blue   = 0;
    } else {
        Carp::carp("Wave length of $wave_length_nm nm is outside of the visible range (380-780 nm)");
        $red = $green = $blue = 0;
    }

    my $factor;

    if ( ( 380 <= $wave_length_nm ) && ( $wave_length_nm < 420 ) ) {
        $factor = 0.3 + 0.7 * ( $wave_length_nm - 380 ) / ( 420 - 380 );
    } elsif ( ( 420 <= $wave_length_nm ) && ( $wave_length_nm < 700 ) ) {
        $factor = 1;
    } elsif ( ( 700 <= $wave_length_nm ) && ( $wave_length_nm <= 780 ) ) {
        $factor = 0.3 + 0.7 * ( 780 - $wave_length_nm ) / ( 780 - 700 );
    } else {
        $factor = 0;
    }

    return map {
        ( $_ * $factor ) ** $gamma
    } ( $red, $green, $blue );
}

my @wavelengths = map { 299792.458 / (440 * (2 ** ($_ / 12))) } -2 .. 9;
my @rainbow;
for my $nm (@wavelengths) {
    for (1 .. 10) {
        push @rainbow =>
            sprintf '%3d %3d %3d',
                map {
                    POSIX::round( $_ * 255 )
                } convert_wave_length_nm_to_rgb( $nm );
    }
}

print << "END_PPM";
P3
@{[ scalar @rainbow ]} 10
255
END_PPM

say join( "\t", @rainbow )
    for 1 .. 10;
