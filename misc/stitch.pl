#!/usr/bin/env perl
use 5.036;

use File::Basename qw(basename);
use File::Spec ();
use File::Temp ();
use FindBin qw($RealBin);
use Getopt::Long qw(GetOptions);
use IPC::Run qw(run);
use Scalar::Util qw(looks_like_number);

# external commands
use constant MIDICSV        => 'midicsv';
use constant FFMPEG         => 'ffmpeg';
use constant PIANOTEQ       => '/Applications/Pianoteq 8/Pianoteq 8.app/Contents/MacOS/Pianoteq 8';
use constant PIANOLIZER     => File::Spec->catfile($RealBin, '..', 'pianolizer');

# params
use constant SAMPLE_RATE    => 48_000;
use constant BUFFER_SIZE    => 480;
use constant BINS           => 115;
use constant REFERENCE      => 48;
use constant KEYBOARD_SIZE  => 88;

# MIDI record structure
use constant MIDI_TRACK     => 0;
use constant MIDI_TIME      => 1;
use constant MIDI_TYPE      => 2;
use constant MIDI_CHANNEL   => 3;
use constant MIDI_NOTE      => 4;
use constant MIDI_VELOCITY  => 5;

use constant MIDI_NOTE_ON_C => 'NOTE_ON_C';
use constant MIDI_NOTE_OFF_C=> 'NOTE_OFF_C';
use constant MIDI_HEADER    => 'HEADER';
use constant MIDI_TEMPO     => 'TEMPO';

sub load_midi($filename) {
    my @command = (MIDICSV ,=> $filename);
    open(my $pipe, '-|', @command)
        or die "Can't pipe from [midicsv '$filename']: $!\n";

    my @midi_data = ();
    while (my $line = readline $pipe) {
        chomp $line;
        my @row = split m{\s*,\s*}x, $line;
        push @midi_data, \@row;
    }
    close $pipe;

    my @midi_data_sorted = sort {
        ($a->[MIDI_TIME] <=> $b->[MIDI_TIME]) || ($a->[MIDI_CHANNEL] <=> $b->[MIDI_CHANNEL])
    } grep {
        looks_like_number($_->[MIDI_TIME]) && looks_like_number($_->[MIDI_CHANNEL])
    } @midi_data;
    @midi_data = ();

    my $tempo = 500_000;
    my $division = 960;
    my $ticks = 0.0;
    my $seconds = 0.0;

    my $switch = {
        MIDI_HEADER ,=> sub ($event) {
            # clock pulses per quarter note
            $division = $event->[MIDI_VELOCITY];
        },
        MIDI_TEMPO ,=> sub ($event) {
            # seconds per quarter note
            $tempo = $event->[MIDI_CHANNEL];
        },
        MIDI_NOTE_ON_C ,=> sub ($event) {
            push @midi_data, $event;
        },
        MIDI_NOTE_OFF_C ,=> sub ($event) {
            $event->[MIDI_TYPE] = MIDI_NOTE_ON_C;
            $event->[MIDI_VELOCITY] = 0;
            push @midi_data, $event;
        },
    };

    for my $event (@midi_data_sorted) {
        # convert time to seconds
        my $delta = $event->[MIDI_TIME] - $ticks;
        $seconds += $delta * ($tempo / $division / 1_000_000) if $delta;
        $ticks = $event->[MIDI_TIME];
        $event->[MIDI_TIME] = $seconds;

        if (my $cb = $switch->{uc($event->[MIDI_TYPE])}) {
            $cb->($event);
        }
    }

    die "Unable to parse '$filename'\n" unless @midi_data;
    return \@midi_data;
}

sub midi_matrix($data) {
    my $step = SAMPLE_RATE / BUFFER_SIZE;
    my $length = $step * $data->[-1]->[MIDI_TIME];
    my @roll = ();
    my $frame = [(0) x KEYBOARD_SIZE];
    for (my $ticks = 0; $ticks <= $length; $ticks++) {
        my $seconds = $ticks / $step;
        while ($data->[0]->[MIDI_TIME] < $seconds) {
            my $event = shift @$data;
            $frame->[$event->[MIDI_NOTE]] = $event->[MIDI_VELOCITY] / 127;
        }
        push @roll, [@$frame];
    }

    return \@roll;
}

sub render_midi($filename) {
    my $wav = File::Temp->new(SUFFIX => '.wav');
    my $buffer;

    my @pianoteq = (
        PIANOTEQ,
        '--headless',
        '--multicore'   => 'max',
        '--midi'        => $filename,
        '--preset'      => 'HB Steinway D Prelude',
        '--bit-depth'   => 32,
        '--mono',
        '--rate'        => SAMPLE_RATE,
        '--wav'         => $wav->filename,
    );

    run \@pianoteq => '>&' => \$buffer;

    my @ffmpeg = (
        FFMPEG,
        '-loglevel'     => 'fatal',
        '-i'            => $wav->filename,
        '-ac'           => 1,
        '-af'           => 'asubcut=27,asupercut=20000',
        '-ar'           => SAMPLE_RATE,
        '-f'            => 'f32le',
        '-c:a'          => 'pcm_f32le',
        '-',
    );

    my @pianolizer = (
        PIANOLIZER,
        '-a'            => 0,
        '-b'            => BUFFER_SIZE,
        '-k'            => BINS,
        '-r'            => REFERENCE,
        '-s'            => SAMPLE_RATE,
        '-y',
        '-d',
    );

    run \@ffmpeg => '|' => \@pianolizer => \$buffer;

    return $buffer;
}

sub pianolizer_matrix($data) {
    my @roll = ();

    for my $line (split m{\n}sx, $data) {
        chomp $line;
        my @frame = split m{\s+}x, $line;
        push @roll, \@frame;
    }

    return \@roll;
}

sub main() {
    GetOptions(
        'input=s'       => \my $input,
        'output=s'      => \my $output,
        'image'         => \my $image,
    );
    my $extension = $image ? '.pgm' : '.dat';
    $output ||= basename($input) =~ s{ \. \w+ $ }{$extension}rx;

    my $midi_data = load_midi($input);
    my $midi_matrix = midi_matrix($midi_data);

    my $audio_data = render_midi($input);
    my $audio_matrix = pianolizer_matrix($audio_data);

    open(my $fh, '>', $output)
        or die "Can't write to $output: $!\n";

    if ($image) {
        printf $fh "P2\n%d\n%d\n255\n", BINS + KEYBOARD_SIZE, scalar(@$midi_matrix);
    }

    for (my $i = 0; $i <= $#$midi_matrix; $i++) {
        my @row = ($audio_matrix->[$i]->@*, $midi_matrix->[$i]->@*);
        if ($image) {
            say $fh join(' ', map { sprintf '%3d', $_ * 255 } @row);
        } else {
            say $fh join(' ', map { sprintf '%.06f', $_ } @row);
        }
    }

    close $fh;
    return 0;
}

exit main();