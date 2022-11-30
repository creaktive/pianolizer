#!/usr/bin/env perl
use 5.036;

package MIDIEvent {
    use Moo;
    use Types::Standard qw(Bool Int Num);

    has channel     => (is => 'ro', isa => Int, default => sub { 1 });
    has factor      => (is => 'ro', isa => Num, required => 1);
    has key         => (is => 'ro', isa => Int, required => 1);
    has state       => (is => 'ro', isa => Bool, required => 1);
    has time        => (is => 'ro', isa => Int, required => 1);
    has velocity    => (is => 'ro', isa => Num, required => 1);

    sub serialize($self) {
        # midicsv format
        # https://www.fourmilab.ch/webtools/midicsv/
        return [
            $self->channel,
            _round($self->factor * $self->time),
            ($self->state ? 'Note_on_c' : 'Note_off_c'),
            0,
            21 + $self->key,
            _round($self->velocity * 127),
        ];
    }

    sub _round($n) { return 0 + sprintf('%.0f', $n) }
}

package TransientDetector {
    use Moo;
    use Types::Standard qw(ArrayRef Int Num Object);

    has key         => (is => 'ro', isa => Int, required => 1);

    has channel     => (is => 'ro', isa => Int, default => sub { 1 });
    has buffer_size => (is => 'ro', isa => Int, default => sub { 554 });
    has division    => (is => 'ro', isa => Int, default => sub { 960 });
    has sample_rate => (is => 'ro', isa => Int, default => sub { 46536 });
    has tempo       => (is => 'ro', isa => Int, default => sub { 500_000 });

    has count       => (is => 'rw', isa => Int, default => sub { 0 });
    has last        => (is => 'rw', isa => Num, default => sub { 0 });
    has start       => (is => 'rw', isa => Int);
    has sum         => (is => 'rw', isa => Num, default => sub { 0 });
    has total       => (is => 'rw', isa => Int, default => sub { 0 });

    has events      => (is => 'ro', isa => ArrayRef[Object], default => sub { [] });
    has factor      => (is => 'lazy', isa => Num);

    sub _build_factor($self) {
        my $pcm_factor = $self->buffer_size / $self->sample_rate;
        my $midi_factor = $self->tempo / $self->division / 1_000_000;
        return $pcm_factor / $midi_factor;
    }

    sub process($self, $sample) {
        $sample = sqrt($sample);
        if (!$self->last && $sample) {
            $self->start($self->total);
            $self->sum($sample);
            $self->count(1);
        } elsif ($sample) {
            $self->sum($self->sum + $sample);
            $self->count($self->count + 1);
        } elsif ($self->last && !$sample) {
            $self->_generate_events;
        }
        $self->last($sample);
        $self->total($self->total + 1);
        return;
    }

    sub finalize($self) {
        $self->_generate_events if $self->count;
        return;
    }

    sub _generate_events($self) {
        my @common_opts = (
            channel     => $self->channel,
            key         => $self->key,
            factor      => $self->factor,
        );
        push $self->events->@* => MIDIEvent->new(
            @common_opts,
            state       => 1,
            time        => $self->start,
            velocity    => $self->sum / $self->count,
        );
        push $self->events->@* => MIDIEvent->new(
            @common_opts,
            state       => 0,
            time        => $self->total,
            velocity    => 0.5,
        );
        $self->count(0);
        return;
    }
}

package main {
    use File::Basename qw(basename);
    use File::Spec ();
    use FindBin qw($RealBin);
    use Getopt::Long qw(GetOptions);
    use IPC::Run qw(run);
    use Term::ProgressBar ();

    sub main() {
        my $BUFFER_SIZE = 554;
        my $CHANNEL     = 1;
        my $CSVMIDI     = 'csvmidi';
        my $DIVISION    = 960;
        my $FFMPEG      = 'ffmpeg';
        my $FILTERS     = 'asubcut=27,asupercut=20000';
        my $INPUT       = 'audio/chromatic.mp3';
        my $KEYS        = 88;
        my $MIDI        = 1;
        my $OUTPUT;
        my $PIANOLIZER  = File::Spec->catfile($RealBin, '..', 'pianolizer');
        my $REFERENCE   = 48;
        my $SAMPLE_RATE = 46536;
        my $SMOOTHING   = 0.04;
        my $THRESHOLD   = 0.05;

        GetOptions(
            'buffer_size=i' => \$BUFFER_SIZE,
            'csvmidi=s'     => \$CSVMIDI,
            'ffmpeg=s'      => \$FFMPEG,
            'filters=s'     => \$FILTERS,
            'input=s'       => \$INPUT,
            'keys=i'        => \$KEYS,
            'midi!'         => \$MIDI,
            'output=s'      => \$OUTPUT,
            'pianolizer=s'  => \$PIANOLIZER,
            'reference=i'   => \$REFERENCE,
            'sample_rate=i' => \$SAMPLE_RATE,
            'smoothing=f'   => \$SMOOTHING,
            'threshold=f'   => \$THRESHOLD,
        );

        $OUTPUT ||= basename($INPUT) =~ s{\.[^\.]+$}{.mid}rx;
        die "'$OUTPUT' already exists!\n" if $MIDI && -e $OUTPUT;
        die "'$PIANOLIZER' not an executable!\n" unless -x $PIANOLIZER;

        my @ffmpeg = (
            $FFMPEG,
            '-loglevel' => 'info',
            '-i'        => $INPUT,
            '-ac'       => 1,
            '-af'       => $FILTERS,
            '-ar'       => $SAMPLE_RATE,
            '-f'        => 'f32le',
            '-c:a'      => 'pcm_f32le',
            '-',
        );
        my @pianolizer = (
            $PIANOLIZER,
            '-a'        => $SMOOTHING,
            '-b'        => $BUFFER_SIZE,
            '-k'        => $KEYS,
            '-r'        => $REFERENCE,
            '-s'        => $SAMPLE_RATE,
            '-t'        => $THRESHOLD,
        );
        run \@ffmpeg => '|' => \@pianolizer => \my $buffer, '2>' => \&progress;

        my @detectors = map {
            TransientDetector->new(
                key         => $_,
                channel     => $CHANNEL,
                division    => $DIVISION,
                sample_rate => $SAMPLE_RATE,
                buffer_size => $BUFFER_SIZE,
            )
        } 0 .. ($KEYS - 1);

        my @buffer = split m{\n}x, $buffer;
        undef $buffer;

        my $progress = Term::ProgressBar->new({
            count   => $#buffer,
            name    => 'Process',
            remove  => 1,
        });

        my $n = 0;
        for my $line (@buffer) {
            my @levels = map { $_ / 255 } unpack 'C*' => pack 'H*' => $line;
            die "WTF\n" if $KEYS != scalar @levels;

            $detectors[$_]->process($levels[$_]) for 0 .. ($KEYS - 1);

            $progress->update($n) if $n++ % 100 == 0;
        }
        $progress->update($#buffer);
        @buffer = ();
        $_->finalize for @detectors;

        my @header = (
            [0, 0, 'Header', 0, 1, $DIVISION],
            [$CHANNEL, 0, 'Start_track'],
            # [$CHANNEL, 0, 'Title_t', '"\000"'],
            # [$CHANNEL, 0, 'Time_signature', 4, 2, 36, 8],
        );
        my @midi = generate_midi(\@detectors);
        @detectors = ();
        my @footer = (
            [$CHANNEL, $midi[-1]->[1], 'End_track'],
            [0, 0, 'End_of_file'],
        );

        $buffer = join "\n", map { join ',' => @$_ } @header, @midi, @footer;
        if ($MIDI) {
            my @csvmidi = ($CSVMIDI => '-' => $OUTPUT);
            run \@csvmidi, \$buffer;
        } else {
            say $buffer;
        }
        return 0;
    }

    sub generate_midi ($detectors) {
        return map {
            $_->serialize
        } sort {
            ($a->time <=> $b->time) || ($a->key <=> $b->key)
        } map {
            $_->events->@*
        } @$detectors;
    }

    sub progress ($line) {
        state $progress;
        my $timestamp = qr{ (\d{2,}) : (\d{2}) : (\d{2}) \. (\d{2}) }x;
        if ($line =~ m{ \b Duration: \s+ $timestamp \b }x) {
            $progress = Term::ProgressBar->new({
                count   => $1 * 3600 + $2 * 60 + $3 + $4 / 100,
                name    => 'Preprocess',
                remove  => 1,
            });
        } elsif ($progress && $line =~ m{ \b time = $timestamp \b }x) {
            $progress->update($1 * 3600 + $2 * 60 + $3 + $4 / 100);
        }
        return;
    }

    exit main();
}