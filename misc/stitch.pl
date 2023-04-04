#!/usr/bin/env perl
use 5.036;

use File::Spec ();
use File::Temp ();
use FindBin qw($RealBin);
use Getopt::Long qw(GetOptions);
use IPC::Run qw(run);

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

    my $on = 0;
    my $off = 0;

    my $tempo = 500_000;
    my $division = 960;

    my @midi_data = ();
    while (my $line = readline $pipe) {
        chomp $line;
        my @row = split m{\s*,\s*}x, $line;
        $row[MIDI_TRACK] += 0;
        $row[MIDI_TYPE] = uc $row[MIDI_TYPE];
        push @midi_data, \@row;
        if ($row[MIDI_TYPE] eq MIDI_NOTE_ON_C) {
            ++$on;
        } elsif ($row[MIDI_TYPE] eq MIDI_NOTE_OFF_C) {
            ++$off;
        } elsif ($row[MIDI_TYPE] eq MIDI_HEADER) {
            # clock pulses per quarter note
            $division = $row[MIDI_VELOCITY];
        } elsif ($row[MIDI_TYPE] eq MIDI_TEMPO) {
            # seconds per quarter note
            $tempo = $row[MIDI_CHANNEL];
        }
        # convert time to seconds
        $row[MIDI_TIME] = $row[MIDI_TIME] * ($tempo / $division / 1_000_000);
    }
    close $pipe;

    @midi_data = sort {
        ($a->[MIDI_TIME] <=> $b->[MIDI_TIME]) || ($a->[MIDI_CHANNEL] <=> $b->[MIDI_CHANNEL])
    } grep {
        (($_->[MIDI_TYPE] eq MIDI_NOTE_ON_C) || ($_->[MIDI_TYPE] eq MIDI_NOTE_OFF_C))
        && !($_->[MIDI_NOTE] == 0 && $_->[MIDI_VELOCITY] == 0)
    } @midi_data;

    die "Unable to parse '$filename'\n" unless @midi_data;
    warn "Note on/off mismatch for $filename: $on NOTE_ON_C but $off NOTE_OFF_C found!\n"
        if $on != $off;

    return \@midi_data;
}

sub midi_matrix($data) {
    my $step = SAMPLE_RATE / BUFFER_SIZE;
    my $length = $step * $data->[-1]->[MIDI_TIME];
    my @roll = ();
    my $frame = [(0) x KEYBOARD_SIZE];
    for (my $clock = 0; $clock <= $length; $clock++) {
        my $time = $clock / $step;
        while ($data->[0]->[MIDI_TIME] < $time) {
            my $event = shift @$data;
            if ($event->[MIDI_TYPE] eq MIDI_NOTE_ON_C) {
                $frame->[$event->[MIDI_NOTE]] = $event->[MIDI_VELOCITY] / 127;
            } elsif ($event->[MIDI_TYPE] eq MIDI_NOTE_OFF_C) {
                $frame->[$event->[MIDI_NOTE]] = 0;
            } else {
                die "WTF\n";
            }
        }
        push @roll, [@$frame];
    }

    return \@roll;
}

sub render_midi($filename) {
    my $wav = File::Temp->new(SUFFIX => '.wav');

    run(
        PIANOTEQ,
        '--quiet',
        '--headless',
        '--midi'        => $filename,
        '--preset'      => 'HB Steinway D Prelude',
        '--bit-depth'   => 32,
        '--mono',
        '--rate'        => SAMPLE_RATE,
        '--wav'         => $wav->filename,
    );

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
    );

    run \@ffmpeg => '|' => \@pianolizer => \my $buffer;

    return $buffer;
}

sub pianolizer_matrix($data) {
    my @roll = ();

    for my $line (split m{\n}sx, $data) {
        chomp $line;
        my @frame = map { $_ / 255 } unpack('C*', pack('H*', $line));
        push @roll, \@frame;
    }

    return \@roll;
}

sub main() {
    GetOptions(
        'midi=s'        => \my $midi,
        'image'         => \my $image,
    );

    my $midi_data = load_midi($midi);
    my $midi_matrix = midi_matrix($midi_data);

    my $audio_data = render_midi($midi);
    my $audio_matrix = pianolizer_matrix($audio_data);

    if ($image) {
        printf "P2\n%d\n%d\n255\n", BINS + KEYBOARD_SIZE, scalar(@$midi_matrix);
    }

    for (my $i = 0; $i <= $#$midi_matrix; $i++) {
        my @row = ($audio_matrix->[$i]->@*, $midi_matrix->[$i]->@*);
        if ($image) {
            say join(' ', map { sprintf '%.0f', $_ * 255 } @row);
        } else {
            say join(' ', map { sprintf '%.06f', $_ } @row);
        }
    }

    return 0;
}

exit main();