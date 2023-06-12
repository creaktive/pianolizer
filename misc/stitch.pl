#!/usr/bin/env perl
use 5.036;

use Digest::MD5 ();
use File::Basename qw(basename);
use File::Spec ();
use File::Temp ();
use FindBin qw($RealBin);
use Getopt::Long qw(GetOptions);
use IO::Compress::Bzip2 qw(bzip2);
use IPC::Run qw(run);

# external commands
use constant MIDICSV        => 'midicsv';
use constant FFMPEG         => 'ffmpeg';
use constant PIANOTEQ       => '/Applications/Pianoteq 8/Pianoteq 8.app/Contents/MacOS/Pianoteq 8';
use constant PIANOLIZER     => File::Spec->catfile($RealBin, '..', 'pianolizer');

# params
use constant SAMPLE_RATE    => 48_000;
use constant BUFFER_SIZE    => 480;
use constant BINS           => 100;
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
use constant MIDI_START_TRACK => 'START_TRACK';
use constant MIDI_TEMPO     => 'TEMPO';

sub load_midi($filename) {
    my @command = (MIDICSV ,=> $filename);
    open(my $pipe, '-|', @command)
        or die "Can't pipe from [midicsv '$filename']: $!\n";

    my @midi_data = ();
    my $tracks = 0;
    my $tempo = 500_000;
    my $division = 960;
    my $ticks = 0.0;
    my $seconds = 0.0;

    my $switch = {
        MIDI_HEADER ,=> sub ($event) {
            # clock pulses per quarter note
            $division = $event->[MIDI_VELOCITY];
        },
        MIDI_START_TRACK ,=> sub ($event) {
            die "Only one track per file is supported!\n"
                if $tracks++;
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

    while (my $line = readline $pipe) {
        chomp $line;
        my @event = split m{\s*,\s*}x, $line;

        # convert time to seconds
        my $delta = $event[MIDI_TIME] - $ticks;
        $seconds += $delta * ($tempo / $division / 1_000_000) if $delta;
        $ticks = $event[MIDI_TIME];
        $event[MIDI_TIME] = $seconds;

        if (my $cb = $switch->{uc($event[MIDI_TYPE])}) {
            $cb->(\@event);
        }
    }

    close $pipe;
    die "Unable to parse '$filename'\n" unless @midi_data;
    return \@midi_data;
}

sub midi_matrix($data, $velocity = 0) {
    my $step = SAMPLE_RATE / BUFFER_SIZE;
    my $length = $step * $data->[-1]->[MIDI_TIME];
    my @roll = ();
    my $frame = [(0) x KEYBOARD_SIZE];
    for (my $ticks = 0; $ticks <= $length; $ticks++) {
        my $seconds = $ticks / $step;
        while ($data->[0]->[MIDI_TIME] < $seconds) {
            my $event = shift @$data;
            my $note = $event->[MIDI_NOTE];
            my $key = $note - 21;
            next if $key >= KEYBOARD_SIZE;
            $frame->[$key] = $velocity
                ? $event->[MIDI_VELOCITY] / 127
                : !!$event->[MIDI_VELOCITY];
        }
        push @roll, [@$frame];
    }

    return \@roll;
}

sub midi_digest($filename) {
    open(my $fh, '<:raw', $filename)
        or die "Can't read from $filename: $!\n";
    my $md5 = Digest::MD5->new->addfile($fh);
    close $fh;
    return $md5;
}

sub render_midi($filename, $preset) {
    my $dir = File::Spec->catdir(File::Spec->tmpdir, 'stitch');
    mkdir $dir unless -d $dir;

    my @synth_params = (
        '--preset'      => $preset,
        '--rate'        => SAMPLE_RATE,
    );

    my $digest = midi_digest($filename);
    $digest->add(join('|', @synth_params));

    my $audio = File::Spec->catfile($dir, $digest->hexdigest . '.flac');
    unless (-f $audio) {
        my $tmp = $audio . '.tmp';

        my @pianoteq = (
            PIANOTEQ,
            '--headless',
            '--mono',
            '--multicore'   => 'max',
            '--midi'        => $filename,
            '--flac'        => $tmp,
            @synth_params,
        );

        run(\@pianoteq => '>&' => \my $buffer)
            && rename($tmp => $audio);
    }

    my @ffmpeg = (
        FFMPEG,
        '-loglevel'     => 'fatal',
        '-i'            => $audio,
        '-ac'           => 1,
        # '-af'           => 'asubcut=27,asupercut=20000',
        '-ar'           => SAMPLE_RATE,
        '-f'            => 'f32le',
        '-c:a'          => 'pcm_f32le',
        '-',
    );

    my @pianolizer = (
        PIANOLIZER,
        '-a'            => 0, # BUFFER_SIZE / SAMPLE_RATE,
        '-b'            => BUFFER_SIZE,
        '-c'            => 1,
        '-k'            => BINS,
        '-r'            => REFERENCE,
        '-s'            => SAMPLE_RATE,
        '-y',
        '-d',
    );

    run \@ffmpeg => '|' => \@pianolizer => \my $buffer;
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

sub stitch($audio_matrix, $midi_matrix, $output, $image = 0, $compress = 0) {
    open(my $fh, '>', $output)
        or die "Can't write to $output: $!\n";

    if ($image) {
        printf $fh "P2\n%d\n%d\n255\n", BINS + KEYBOARD_SIZE, scalar(@$midi_matrix);
    }

    for (my $i = 0; $i <= $#$midi_matrix; $i++) {
        my @row = ($audio_matrix->[$i]->@*, $midi_matrix->[$i]->@*);
        say $fh $image
            ? join(' ', map { sprintf '%3d', $_ * 255 } @row)
            : join(' ', map { sprintf '%.06f', $_ } @row);
    }

    close $fh;

    if ($compress) {
        bzip2($output, $output . '.bz2')
            && unlink($output);
    }
}

sub main() {
    GetOptions(
        'input=s'       => \my $input,
        'output=s'      => \my $output,
        'image'         => \my $image,
        'compress'      => \my $compress,
        'overwrite'     => \my $overwrite,
        'preset=s'      => \my $preset,
        'velocity'      => \my $velocity,
    );
    my $extension = $image ? '.pgm' : '.dat';
    $output ||= basename($input) =~ s{ \. \w+ $ }{$extension}rx;
    if (!$overwrite && (-f $output || -f $output . '.bz2')) {
        warn "$output already exists!\n";
        return 0;
    }
    $preset ||= 'HB Steinway Model D';

    my $midi_data = load_midi($input);
    my $midi_matrix = midi_matrix($midi_data, $velocity);

    my $audio_data = render_midi($input, $preset);
    my $audio_matrix = pianolizer_matrix($audio_data);

    stitch($audio_matrix, $midi_matrix, $output, $image, $compress);

    return 0;
}

exit main();